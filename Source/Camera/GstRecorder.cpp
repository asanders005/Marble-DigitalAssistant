#include "GstRecorder.h"

#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <iostream>
#include <sstream>

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

    this->filename = filename;
    this->width = width;
    this->height = height;
    this->fps = fps;
    this->bitrate_kbps = bitrate_kbps;
    this->maxQueueSize = maxQueueSize;

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
    appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "src");
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

    // Ensure appsrc blocks when queue is full
    g_object_set(G_OBJECT(appsrc), "stream-type", GST_APP_STREAM_TYPE_STREAM, NULL);

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
    return true;
}

bool GstRecorder::pushFrame(const cv::Mat& frame)
{
    if (!running.load()) return false;

    std::lock_guard<std::mutex> lk(queueMutex);
    if (frameQueue.size() >= maxQueueSize)
    {
        // Drop frame if queue is full
        return false;
    }
    frameQueue.push_back(frame.clone());
    queueCondVar.notify_one();
    return true;
}

void GstRecorder::stop()
{
    if (!running.load()) return;

    // signal the thread to stop
    stopRequested.store(true);
    queueCondVar.notify_one();
    if (recorderThread.joinable())
        recorderThread.join();

    // push EOS to appsrc
    if (appsrc)
        gst_app_src_end_of_stream(GST_APP_SRC(appsrc));

    gst_element_send_event(pipeline, gst_event_new_eos());
    gst_element_set_state(pipeline, GST_STATE_NULL);

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

    running.store(false);

    std::lock_guard<std::mutex> lk(queueMutex);
    frameQueue.clear();
}

void GstRecorder::recorderThreadFunc()
{
    while (!stopRequested_) {
        cv::Mat frame;
        {
            std::unique_lock<std::mutex> lk(queueMutex_);
            if (queue_.empty()) {
                queueCv_.wait_for(lk, std::chrono::milliseconds(100));
                if (queue_.empty()) continue;
            }
            frame = std::move(queue_.front());
            queue_.pop_front();
        }
        if (frame.empty()) continue;

        // compute pts based on frame index and fps
        GstClockTime pts = gst_util_uint64_scale(frameIndex_, GST_SECOND, static_cast<int>(std::round(fps_)));
        GstBuffer *buf = matToGstBuffer(frame, pts);
        if (!buf) {
            std::cerr << "Failed to create GstBuffer\n";
            continue;
        }
        GstFlowReturn fret = gst_app_src_push_buffer(GST_APP_SRC(appsrc_), buf);
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
            ++frameIndex_;
        }
    }

    // end: nothing left to do; worker will exit and stop() will send EOS
}

std::string GstRecorder::buildPipelineString()
{
    std::ostringstream ss;
    ss << "appsrc name=src is-live=true format=time do-timestamp=true block=true "
       << "! videoconvert ! queue ! x264enc bitrate=" << bitrate_kbps_
       << " speed-preset=ultrafast tune=zerolatency ! "
       << "h264parse ! mp4mux ! filesink location=" << filename_;
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
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(GST_SECOND, 1, static_cast<int>(std::round(fps)));
    return buffer;
}