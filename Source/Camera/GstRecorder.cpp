#include "GstRecorder.h"
#include "Metrics/MongoLink.h"

#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <iostream>
#include <sstream>
#include <filesystem>

GstRecorder::GstRecorder()
{
    static bool gst_initialized = false;
    if (!gst_initialized)
    {
        gst_init(nullptr, nullptr);
        gst_initialized = true;
    }
}

GstRecorder::~GstRecorder()
{
    stop();
}

bool GstRecorder::start(const std::string& filename, int width, int height, double fps, int bitrate_kbps, size_t maxQueueSize)
{
    if (running.load()) return false;

    configure(width, height, fps, bitrate_kbps, maxQueueSize);
    return start(filename);
}

bool GstRecorder::start(const std::string& filename)
{
    if (running.load()) return false;

    this->filename = filename;

    frameIndex = 0;
    stopRequested.store(false);

    std::string pipelineStr = buildPipelineString();
    GError* error = nullptr;
    pipeline = gst_parse_launch(pipelineStr.c_str(), &error);
    if (!pipeline)
    {
        std::cerr << "gst_parse_launch failed: " << (error ? error->message : "unknown error") << std::endl;
        if (error) g_error_free(error);
        return false;
    }

    // Get appsrc element by name ("src")
    appsrc = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(pipeline), "src"));
    if (!appsrc)
    {
        std::cerr << "Failed to get appsrc from pipeline\n";
        gst_object_unref(pipeline);
        pipeline = nullptr;
        return false;
    }

    // Configure caps: BGR
    GstCaps* caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "BGR",
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        "framerate", GST_TYPE_FRACTION, static_cast<gint>(std::round(fps)), 1,
        NULL);
    gst_app_src_set_caps(GST_APP_SRC(appsrc), caps);
    gst_caps_unref(caps);

    // Ensure appsrc blocks when queue is full and use time format for buffers
    g_object_set(G_OBJECT(appsrc), "stream-type", GST_APP_STREAM_TYPE_STREAM, NULL);
    g_object_set(G_OBJECT(appsrc), "format", GST_FORMAT_TIME, NULL);

    // Precompute frame duration (use floating math for fractional fps)
    if (fps > 0.0)
    {
        frameDuration = static_cast<GstClockTime>((double)GST_SECOND / fps + 0.5);
    }
    else
    {
        frameDuration = GST_SECOND / 30; // fallback
    }

    // establish base time for timestamping (steady clock)
    baseTime = std::chrono::steady_clock::now();

    // Set pipeline state to PLAYING
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        std::cerr << "Failed to set pipeline to PLAYING state\n";
        gst_object_unref(appsrc);
        gst_object_unref(pipeline);
        appsrc = nullptr;
        pipeline = nullptr;
        return false;
    }

    running.store(true);
    recorderThread = std::thread(&GstRecorder::recorderThreadFunc, this);

    // diagnostic print
    std::cerr << "[GstRecorder] started filename='" << filename << "' fps=" << fps << " frameDuration=" << frameDuration << " ns\n";
    return true;
}

bool GstRecorder::pushFrame(const cv::Mat& frame)
{
    if (!running.load()) return false;

    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(queueMutex);
    if (frameQueue.size() >= maxQueueSize)
    {
        // Drop frame if queue is full
        return false;
    }
    frameQueue.emplace_back(frame.clone(), now);
    queueCondVar.notify_one();
    return true;
}

void GstRecorder::stop(bool upload)
{
    if (!running.load()) return;

    // signal the thread to stop
    stopRequested.store(true);
    queueCondVar.notify_one();
    if (recorderThread.joinable())
        recorderThread.join();

    // Push EOS to appsrc so the pipeline can finish and mp4mux can write the file tail
    if (appsrc)
        gst_app_src_end_of_stream(GST_APP_SRC(appsrc));

    // Also send an EOS event to the pipeline and wait for the EOS or ERROR message on the bus.
    if (pipeline)
    {
        gst_element_send_event(pipeline, gst_event_new_eos());

        GstBus* bus = gst_element_get_bus(pipeline);
        if (bus)
        {
            // wait up to 5s for EOS/ERROR (adjust timeout if needed)
            GstMessage* msg = gst_bus_timed_pop_filtered(bus, 5 * GST_SECOND,
                static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
            if (msg)
            {
                if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
                {
                    GError* err = nullptr;
                    gst_message_parse_error(msg, &err, nullptr);
                    std::cerr << "GStreamer pipeline error during EOS: " << (err ? err->message : "(unknown)") << std::endl;
                    if (err) g_error_free(err);
                }
                gst_message_unref(msg);
            }
            gst_object_unref(bus);
        }

        // Now stop the pipeline cleanly
        gst_element_set_state(pipeline, GST_STATE_NULL);
    }

    if (appsrc)
    {
        gst_object_unref(appsrc);
        appsrc = nullptr;
    }
    if (pipeline)
    {
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }

    if (upload)
    {
        // Upload video to MongoDB
        MongoLink::GetInstance().UploadVideo(filename, std::filesystem::path(filename).filename().string());
    }
    running.store(false);

    std::lock_guard<std::mutex> lk(queueMutex);
    frameQueue.clear();
}

void GstRecorder::recorderThreadFunc()
{
    while (!stopRequested.load()) {
        cv::Mat frame;
        TimePoint frameStamp;
        {
            std::unique_lock<std::mutex> lk(queueMutex);
            if (frameQueue.empty()) {
                queueCondVar.wait_for(lk, std::chrono::milliseconds(100));
                if (frameQueue.empty()) continue;
            }
            auto p = std::move(frameQueue.front());
            frame = std::move(p.first);
            frameStamp = p.second;
            frameQueue.pop_front();
        }
        if (frame.empty()) continue;

        // compute PTS from the capture timestamp stored with the frame
        GstClockTime pts = 0;
        GstClockTime duration_ns = 0;
        {
            auto tp = frameStamp;
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(tp - baseTime).count();
            if (ns < 0) ns = 0;
            pts = static_cast<GstClockTime>(ns);
            duration_ns = frameDuration; // keep nominal duration
        }
        GstBuffer *buf = matToGstBuffer(frame, pts);
        if (!buf) {
            std::cerr << "Failed to create GstBuffer\n";
            continue;
        }
        GstFlowReturn fret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buf);
        if (fret != GST_FLOW_OK) {
            std::cerr << "gst_app_src_push_buffer returned " << fret << std::endl;
            // if push fails, free buffer and consider retrying or dropping
            // gst_app_src_push_buffer consumes the buffer on GST_FLOW_OK, otherwise we must unref
            if (fret != GST_FLOW_OK) {
                // not accepted; unref (we already allocated)
                // gst_app_src_push_buffer consumes the buffer on success; on error it's not consumed
                gst_buffer_unref(buf);
            }
            // small sleep to avoid busy-looping on errors
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } else {
            // pushed successfully
            if (debugPrintCount < 5)
            {
                std::cerr << "[GstRecorder] pushed frameIndex=" << frameIndex << " pts=" << pts << " duration=" << frameDuration << "\n";
                ++debugPrintCount;
            }
            ++frameIndex;
        }
    }

    // end: nothing left to do; worker will exit and stop() will send EOS
}

std::string GstRecorder::buildPipelineString()
{
    std::ostringstream ss;
    // Request h264parse to periodically emit SPS/PPS (config-interval=1)
    // and enable faststart on mp4mux so the moov atom is placed for easier playback.
     // Do not set do-timestamp or is-live here — we will stamp buffers explicitly
     ss << "appsrc name=src format=time block=true "
         << "! videoconvert ! queue ! x264enc bitrate=" << bitrate_kbps
       << " speed-preset=ultrafast tune=zerolatency ! "
       << "h264parse config-interval=1 ! mp4mux faststart=true ! filesink location=" << filename;
    return ss.str();
}

GstBuffer* GstRecorder::matToGstBuffer(const cv::Mat& frame, GstClockTime pts)
{
    int bytes = frame.total() * frame.elemSize();
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, bytes, nullptr);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_WRITE))
    {
        // Copy row by row to handle possible padding
        if (frame.isContinuous())
        {
            memcpy(map.data, frame.data, bytes);
        }
        else
        {
            uint8_t* destPtr = map.data;
            for (int i = 0; i < frame.rows; ++i)
            {
                memcpy(destPtr, frame.ptr(i), frame.cols * frame.elemSize());
                destPtr += frame.cols * frame.elemSize();
            }
        }
        gst_buffer_unmap(buffer, &map);
    }
    else
    {
        gst_buffer_unref(buffer);
        return nullptr;
    }

    GST_BUFFER_PTS(buffer) = pts;
    GST_BUFFER_DTS(buffer) = pts;
    GST_BUFFER_DURATION(buffer) = frameDuration;
    return buffer;
}