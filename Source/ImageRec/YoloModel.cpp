#include "YoloModel.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <json.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

YOLOModel::YOLOModel() {}

bool YOLOModel::Initialize(const std::string &modelPath, const std::string &labelsPath,
                           int threadCount)
{
    cv::setNumThreads(threadCount);
    if (!loadModel(modelPath))
    {
        std::cerr << "[YOLOModel::Initialize] Failed to load model: " << modelPath << std::endl;
        return false;
    }
    if (!labelsPath.empty())
    {
        if (!loadLabels(labelsPath))
        {
            std::cerr << "WARNING [YOLOModel::Initialize] Failed to load labels: " << labelsPath
                      << std::endl;
        }
    }
    else
    {
        std::cerr
            << "WARNING [YOLOModel::Initialize] No labels path provided; proceeding without labels."
            << std::endl;
    }
    return true;
}

bool YOLOModel::loadModel(const std::string &modelPath)
{
    try
    {
        net = std::make_unique<cv::dnn::Net>(cv::dnn::readNetFromONNX(modelPath));
        net->setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net->setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    }
    catch (const std::exception &e)
    {
        std::cerr << "[YOLOModel::loadModel] exception: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool YOLOModel::loadLabels(const std::string &labelsPath)
{
    labels.clear();

    std::string fileExt = labelsPath.substr(labelsPath.find_last_of('.') + 1);
    if (fileExt == "yaml" || fileExt == "yml")
    {
        try
        {
            YAML::Node yamlRoot = YAML::LoadFile(labelsPath);
            if (yamlRoot["names"])
            {
                const auto &namesNode = yamlRoot["names"];
                if (namesNode.IsSequence())
                {
                    // std::cout << "[YOLOModel::loadLabels] 'names' node is a sequence;
                    // processing."
                    //           << labelsPath << std::endl;

                    for (const auto &nameNode : namesNode)
                    {
                        if (nameNode.IsScalar())
                        {
                            labels.push_back(nameNode.as<std::string>());
                        }
                    }
                }
                else if (namesNode.IsMap())
                {
                    // std::cout << "[YOLOModel::loadLabels] 'names' node is a map; processing
                    // keys."
                    //           << labelsPath << std::endl;

                    // Collect keys and sort numerically
                    std::vector<int> keys;
                    for (auto it = namesNode.begin(); it != namesNode.end(); ++it)
                    {
                        keys.push_back(it->first.as<int>());
                    }
                    std::sort(keys.begin(), keys.end());
                    for (int k : keys)
                    {
                        labels.push_back(namesNode[k].as<std::string>());
                    }
                }
                else
                {
                    std::cerr
                        << "[YOLOModel::loadLabels] 'names' node is neither sequence nor map: "
                        << labelsPath << std::endl;
                    return false;
                }
            }
            else
            {
                std::cerr << "[YOLOModel::loadLabels] YAML file does not contain node 'names': "
                          << labelsPath << std::endl;
                return false;
            }
        }
        catch (const YAML::Exception &e)
        {
            std::cerr << "[YOLOModel::loadLabels] YAML exception: " << e.what() << std::endl;
            return false;
        }

        std::cout << "[YOLOModel::loadLabels] loaded " << labels.size() << " labels from YAML file."
                  << std::endl;

        return !labels.empty();
    }

    std::ifstream ifs(labelsPath);
    if (!ifs)
        return false;

    if (fileExt == "json" || fileExt == "jsonl")
    {
        nlohmann::json data = nlohmann::json::parse(ifs, nullptr, false);
        if (data.is_discarded())
        {
            std::cerr << "[YOLOModel::loadLabels] Failed to parse JSON label file: " << labelsPath
                      << std::endl;
            return false;
        }
        if (data.is_array())
        {
            for (const auto &item : data)
            {
                if (item.is_string())
                    labels.push_back(item.get<std::string>());
            }
        }

        return !labels.empty();
    }

    std::cerr << "WARNING [YOLOModel::loadLabels] Unknown label file extension: " << fileExt
              << ". Attempting plain text load." << std::endl;
    // Plain text file, one label per line
    std::string line;
    while (std::getline(ifs, line))
    {
        if (!line.empty())
        {
            labels.push_back(line);
        }
    }
    return !labels.empty();
}

std::vector<YoloDetection> YOLOModel::detect(const cv::Mat &img, float confThresh, float iouThresh)
{
    // Use stricter default thresholds if not provided
    if (confThresh < 1e-4f)
        confThresh = 0.5f;
    if (iouThresh < 1e-4f)
        iouThresh = 0.5f;
    std::vector<YoloDetection> results;
    if (!net)
    {
        std::cerr << "[YOLOModel::detect] network not loaded" << std::endl;
        return results;
    }
    if (img.empty())
        return results;

    // Letterbox-resize the image to preserve aspect ratio and map back to original coords
    int origW = img.cols;
    int origH = img.rows;
    int inpW = inputSize.width;
    int inpH = inputSize.height;
    float r = std::min((float)inpW / origW, (float)inpH / origH);
    int newW = std::max(1, (int)std::round(origW * r));
    int newH = std::max(1, (int)std::round(origH * r));
    int padW = inpW - newW;
    int padH = inpH - newH;
    int padLeft = padW / 2;
    int padTop = padH / 2;

    cv::Mat resized = cv::Mat::zeros(inpH, inpW, img.type());
    cv::Mat tmp;
    cv::resize(img, tmp, cv::Size(newW, newH));
    tmp.copyTo(resized(cv::Rect(padLeft, padTop, newW, newH)));

    cv::Mat blob =
        cv::dnn::blobFromImage(resized, 1.0 / 255.0, inputSize, cv::Scalar(), true, false);
    try
    {
        // advance frame counter for tracking/persistence
        ++frameIndex;
        net->setInput(blob);
        cv::Mat out = net->forward();
        if (out.empty())
            return results;

        // Expecting shape (1, num_preds, attrs)
        if (out.dims < 3)
        {
            std::cerr << "[YOLOModel::detect] unexpected output dims=" << out.dims << std::endl;
            return results;
        }
        int num_preds = out.size[1];
        int attrs = out.size[2];
        cv::Mat a = out.reshape(1, num_preds); // rows=num_preds, cols=attrs

        // Debug: log basic stats about outputs and attribute ranges
        float rawMaxConf = 0.f;
        float sigMaxConf = 0.f;
        int countOver001 = 0;
        int countOver01 = 0;
        int countOverThresh = 0;
        std::vector<float> minAttr(attrs, std::numeric_limits<float>::infinity());
        std::vector<float> maxAttr(attrs, -std::numeric_limits<float>::infinity());
        for (int i = 0; i < num_preds; ++i)
        {
            for (int r = 0; r < attrs; ++r)
            {
                float v = a.at<float>(i, r);
                if (v < minAttr[r])
                    minAttr[r] = v;
                if (v > maxAttr[r])
                    maxAttr[r] = v;
            }
            float rawConf = a.at<float>(i, 4);
            float conf = 1.0f / (1.0f + std::exp(-rawConf)); // sigmoid on logits
            if (rawConf > rawMaxConf)
                rawMaxConf = rawConf;
            if (conf > sigMaxConf)
                sigMaxConf = conf;
            if (conf > 0.001f)
                ++countOver001;
            if (conf > 0.01f)
                ++countOver01;
            if (conf > confThresh)
                ++countOverThresh;
        }
        std::cout << std::fixed << std::setprecision(6);
        // std::cout << "[YOLOModel::detect] attrs=" << attrs << " num_preds=" << num_preds
        //           << " rawMaxConf=" << rawMaxConf << " sigMaxConf=" << sigMaxConf
        //           << " >0.001=" << countOver001 << " >0.01=" << countOver01 << " >" << confThresh
        //           << "=" << countOverThresh << std::endl;
        // for (int r = 0; r < attrs; ++r)
        // {
        //     std::cout << " attr[" << r << "] min=" << minAttr[r] << " max=" << maxAttr[r]
        //               << std::endl;
        // }

        std::vector<cv::Rect> boxes;
        std::vector<float> scores;
        std::vector<int> preNmsIdx;

        // Decide whether the coordinates are pixel units or normalized [0..1].
        bool coordsArePixels = false;
        for (int r = 0; r < std::min((int)maxAttr.size(), attrs); ++r)
        {
            if (maxAttr[r] > 1.5f)
            {
                coordsArePixels = true;
                break;
            }
        }
        // if (coordsArePixels)
        //     std::cout << "[YOLOModel::detect] interpreting box coords as pixel units" << std::endl;

        for (int i = 0; i < num_preds; ++i)
        {
            float cx = a.at<float>(i, 0);
            float cy = a.at<float>(i, 1);
            float w = a.at<float>(i, 2);
            float h = a.at<float>(i, 3);
            float rawConf = a.at<float>(i, 4);
            float objConf = 1.0f / (1.0f + std::exp(-rawConf));

            int predictedClass = 0;
            float finalScore = objConf;
            // For YOLOv5/YOLOv8: apply sigmoid to each class logit, not softmax
            if (attrs > 5)
            {
                int numClasses = attrs - 5;
                std::vector<float> cls(numClasses);
                float bestProb = 0.f;
                int best = 0;
                for (int c = 0; c < numClasses; ++c)
                {
                    float logit = a.at<float>(i, 5 + c);
                    float prob = 1.0f / (1.0f + std::exp(-logit)); // sigmoid
                    cls[c] = prob;
                    if (prob > bestProb)
                    {
                        bestProb = prob;
                        best = c;
                    }
                }
                predictedClass = best;
                finalScore = objConf * bestProb; // combine objectness and best class prob
            }
            else
            {
                // single-class or person-only model: predictedClass stays 0
                finalScore = objConf;
            }

            // Debug output for each prediction after class/score calculation
            if (i < 10) {
                std::ostringstream ss;
                ss << "[YOLOModel::detect] pred " << i << ": conf=" << finalScore << ", class=" << predictedClass;
                if (labels.size() > predictedClass && predictedClass >= 0) {
                    ss << " (" << labels[predictedClass] << ")";
                }
                std::cout << ss.str() << std::endl;
            }

            // If labels are present and model is multi-class, require 'person'
            if (labels.size() > 1)
            {
                // find person index if present
                int personIdx = -1;
                for (size_t li = 0; li < labels.size(); ++li)
                {
                    if (labels[li] == "person")
                    {
                        personIdx = (int)li;
                        break;
                    }
                }
                // std::cout << "[YOLOModel::detect] person class index = " << personIdx <<
                // std::endl; std::cout << "[YOLOModel::detect] predicted class index = " <<
                // predictedClass
                //           << std::endl;

                if (personIdx != -1 && predictedClass != personIdx)
                    continue; // skip non-person
            }

            if (finalScore < confThresh)
                continue;

            float bx, by, bw, bh;
            if (coordsArePixels)
            {
                // Treat cx/cy/w/h as pixel units in input image space
                bx = (cx - w / 2.0f - padLeft) / r;
                by = (cy - h / 2.0f - padTop) / r;
                bw = (w) / r;
                bh = (h) / r;
            }
            else
            {
                // Normalized coordinates [0..1]
                bx = (cx * inpW - w * inpW / 2.0f - padLeft) / r; // map to original image coords
                by = (cy * inpH - h * inpH / 2.0f - padTop) / r;
                bw = (w * inpW) / r;
                bh = (h * inpH) / r;
            }

            int x = static_cast<int>(std::round(bx));
            int y = static_cast<int>(std::round(by));
            int iw = static_cast<int>(std::round(bw));
            int ih = static_cast<int>(std::round(bh));
            cv::Rect rbox(x, y, iw, ih);
            // clamp to original image
            rbox &= cv::Rect(0, 0, origW, origH);
            if (rbox.width <= 0 || rbox.height <= 0)
                continue;
            boxes.push_back(rbox);
            scores.push_back(finalScore);
            preNmsIdx.push_back(i);
            // store class id with the score in a parallel structure by encoding
            // as negative index in trackLastScore temporarily is not ideal; instead
            // we'll attach class via trackLastBox mapping after detection.
        }

        // Filter out very small boxes and unlikely aspect ratios (reduce false positives)
        const float imgArea = (float)origW * (float)origH;
        const float minArea = minBoxAreaRatio * imgArea;
        const int minH = std::max(2, (int)std::round(minBoxHeightRatio * origH));
        std::vector<std::tuple<float, cv::Rect, int>> scoredBoxes;
        for (size_t i = 0; i < boxes.size(); ++i)
        {
            const cv::Rect &b = boxes[i];
            if ((float)b.area() < minArea)
                continue;
            if (b.height < minH)
                continue;
            if (b.width > 0 && (b.height / (float)b.width) < 0.6f)
                continue;
            scoredBoxes.emplace_back(scores[i], b, preNmsIdx[i]);
        }
        // Sort by score descending and keep only top 50 before NMS
        std::sort(scoredBoxes.begin(), scoredBoxes.end(),
                  [](const auto &A, const auto &B) { return std::get<0>(A) > std::get<0>(B); });
        if (scoredBoxes.size() > 50)
            scoredBoxes.resize(50);
        boxes.clear();
        scores.clear();
        for (const auto &sb : scoredBoxes)
        {
            scores.push_back(std::get<0>(sb));
            boxes.push_back(std::get<1>(sb));
        }

        // If no boxes yet but model produced some non-zero confidences below
        // the requested threshold, attempt a short debug pass with a much
        // lower temporary threshold so we can visualize candidate detections.
        bool usedPixelFallback = false; // set true if we used the pixel-unit fallback
        if (boxes.empty())
        {
            // Determine whether pixel fallback populated boxes above
            // In the earlier block we only filled boxes for conf>=confThresh,
            // but we also attempted a pixel-unit fallback that appended boxes.
            // We consider 'usedPixelFallback' true if any attribute ranges exceed 1.5
            // times the input size (heuristic).
            if (maxAttr[0] > inpW || maxAttr[1] > inpH)
                usedPixelFallback = true;
        }

        // NMS (normal pass)
        std::vector<int> idxs;
        cv::dnn::NMSBoxes(boxes, scores, confThresh, iouThresh, idxs);
        // Limit to top 10 after NMS
        if (idxs.size() > 10)
            idxs.resize(10);

        // If nothing survived and there were small confidences, try a debug pass
        if (idxs.empty() && sigMaxConf > 0.0f && sigMaxConf < confThresh)
        {
            float confDebug = std::max(1e-6f, std::min(sigMaxConf, confThresh * 0.01f));
            // std::cout << "[YOLOModel::detect] no results with conf=" << confThresh
            //   << "; attempting debug pass with conf=" << confDebug << std::endl;

            std::vector<cv::Rect> dbgBoxes;
            std::vector<float> dbgScores;
            for (int i = 0; i < num_preds; ++i)
            {
                float rawConf = a.at<float>(4, i);
                float conf = 1.0f / (1.0f + std::exp(-rawConf));
                if (conf < confDebug)
                    continue;
                float cx = a.at<float>(0, i);
                float cy = a.at<float>(1, i);
                float w = a.at<float>(2, i);
                float h = a.at<float>(3, i);
                float bx, by, bw, bh;
                if (usedPixelFallback)
                {
                    bx = (cx - w / 2.0f - padLeft) / r;
                    by = (cy - h / 2.0f - padTop) / r;
                    bw = w / r;
                    bh = h / r;
                }
                else
                {
                    bx = (cx * inpW - w * inpW / 2.0f - padLeft) / r;
                    by = (cy * inpH - h * inpH / 2.0f - padTop) / r;
                    bw = (w * inpW) / r;
                    bh = (h * inpH) / r;
                }
                int x = static_cast<int>(std::round(bx));
                int y = static_cast<int>(std::round(by));
                int iw = static_cast<int>(std::round(bw));
                int ih = static_cast<int>(std::round(bh));
                cv::Rect rbox(x, y, iw, ih);
                rbox &= cv::Rect(0, 0, origW, origH);
                if (rbox.width <= 0 || rbox.height <= 0)
                    continue;
                dbgBoxes.push_back(rbox);
                dbgScores.push_back(conf);
            }
            if (!dbgBoxes.empty())
            {
                std::vector<int> dbgIdxs;
                cv::dnn::NMSBoxes(dbgBoxes, dbgScores, confDebug, iouThresh, dbgIdxs);
                std::cerr << "[YOLOModel::detect] debug pass found " << dbgIdxs.size() << " boxes"
                          << std::endl;
            }
        }

        std::vector<YoloDetection> dets;
        for (int id : idxs)
        {
            YoloDetection d;
            d.box = boxes[id];
            d.score = scores[id];
            d.classId = 0;
            d.trackId = -1;
            dets.push_back(d);
        }

        // Better clustering: group detections by center proximity (and small IoU)
        // then aggregate each cluster into a single box only if IoU is high.
        if (dets.size() > 1)
        {
            // Union-find based only on IoU > 0.3
            std::vector<int> parent(dets.size());
            for (size_t i = 0; i < dets.size(); ++i)
                parent[i] = i;
            std::function<int(int)> findp = [&](int a) -> int
            { return parent[a] == a ? a : parent[a] = findp(parent[a]); };
            auto unite = [&](int a, int b)
            {
                a = findp(a);
                b = findp(b);
                if (a != b)
                    parent[b] = a;
            };

            for (size_t i = 0; i < dets.size(); ++i)
            {
                for (size_t j = i + 1; j < dets.size(); ++j)
                {
                    float u = iou(dets[i].box, dets[j].box);
                    if (u > 0.3f)
                        unite((int)i, (int)j);
                }
            }

            // collect clusters
            std::unordered_map<int, std::vector<int>> clusters;
            for (size_t i = 0; i < dets.size(); ++i)
                clusters[findp((int)i)].push_back((int)i);

            std::vector<YoloDetection> merged;
            for (auto &kv : clusters)
            {
                const std::vector<int> &idxsCl = kv.second;
                if (idxsCl.empty())
                    continue;
                // Aggregate by union of member boxes (min x/min y, max x2/max y2)
                int minx = INT_MAX, miny = INT_MAX, maxx = 0, maxy = 0;
                float bestScore = 0.f;
                for (int id : idxsCl)
                {
                    const cv::Rect &b = dets[id].box;
                    minx = std::min(minx, b.x);
                    miny = std::min(miny, b.y);
                    maxx = std::max(maxx, b.x + b.width);
                    maxy = std::max(maxy, b.y + b.height);
                    bestScore = std::max(bestScore, dets[id].score);
                }
                if (minx >= maxx || miny >= maxy)
                    continue;
                cv::Rect nb(minx, miny, maxx - minx, maxy - miny);
                // enforce minimum box height/area and add small padding
                const int minH = std::max(2, (int)std::round(minBoxHeightRatio * origH));
                if (nb.height < minH)
                {
                    int dh = minH - nb.height;
                    nb.y = std::max(0, nb.y - dh / 2);
                    nb.height = std::min(origH - nb.y, minH);
                }
                const float imgArea = (float)origW * (float)origH;
                const float minArea = minBoxAreaRatio * imgArea;
                if ((float)nb.area() < minArea)
                {
                    int targetH = std::max(minH, (int)std::round(std::sqrt(minArea)));
                    int dh = targetH - nb.height;
                    nb.y = std::max(0, nb.y - dh / 2);
                    nb.height = std::min(origH - nb.y, targetH);
                }
                // add small padding (5%) to include context
                int padx = std::max(1, (int)std::round(0.05f * nb.width));
                int pady = std::max(1, (int)std::round(0.05f * nb.height));
                nb.x = std::max(0, nb.x - padx);
                nb.y = std::max(0, nb.y - pady);
                nb.width = std::min(origW - nb.x, nb.width + padx * 2);
                nb.height = std::min(origH - nb.y, nb.height + pady * 2);

                nb &= cv::Rect(0, 0, origW, origH);
                // Filter out boxes covering >60% of the image area (likely spurious)
                float areaFrac = (float)nb.area() / ((float)origW * (float)origH);
                if (nb.width <= 0 || nb.height <= 0 || areaFrac > 0.6f)
                    continue;
                YoloDetection md;
                md.box = nb;
                md.score = bestScore;
                md.classId = 0;
                md.trackId = -1;
                merged.push_back(md);
            }
            dets.swap(merged);
            // std::cerr << "YOLOModel::detect: clustered to " << dets.size() << " boxes" <<
            // std::endl;
        }

        // Simple IoU-based tracking using persistent track map (trackLastBox)
        // First, prevent runaway numbers of detections from flooding the UI by
        // keeping only the top-K by score before matching.
        const size_t kMaxDetections = 200;
        if (dets.size() > kMaxDetections)
        {
            std::sort(dets.begin(), dets.end(),
                      [](const YoloDetection &A, const YoloDetection &B)
                      { return A.score > B.score; });
            dets.resize(kMaxDetections);
        }

        std::unordered_set<int> matchedTracks;
        for (auto &d : dets)
        {
            float bestIoU = 0.f;
            int bestTrack = -1;
            for (const auto &kv : trackLastBox)
            {
                int tid = kv.first;
                const cv::Rect &pb = kv.second;
                float u = iou(d.box, pb);
                if (u > bestIoU)
                {
                    bestIoU = u;
                    bestTrack = tid;
                }
            }
            if (bestIoU > 0.3f && bestTrack != -1)
            {
                d.trackId = bestTrack;
            }
            else
            {
                d.trackId = nextTrackId++;
            }
            // update persistent maps
            trackLastBox[d.trackId] = d.box;
            trackLastScore[d.trackId] = d.score;
            trackLastSeen[d.trackId] = frameIndex;
            matchedTracks.insert(d.trackId);
        }

        // Add recently-seen tracks that were lost this frame (keepAliveFrames)
        std::vector<YoloDetection> finalDets = dets;
        for (const auto &kv : trackLastSeen)
        {
            int tid = kv.first;
            int last = kv.second;
            if (matchedTracks.find(tid) != matchedTracks.end())
                continue;
            int age = frameIndex - last;
            if (age <= keepAliveFrames)
            {
                YoloDetection d;
                d.trackId = tid;
                d.box = trackLastBox[tid];
                // decay score over time so older ghost boxes fade
                float decay = std::pow(0.7f, (float)age);
                d.score = trackLastScore[tid] * decay;
                d.classId = 0;
                finalDets.push_back(d);
            }
        }

        // Save for next-frame matching and return
        prevDetections = finalDets;
        results = finalDets;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[YOLOModel] detect: forward failed: " << e.what() << std::endl;
    }

    return results;
}

float YOLOModel::iou(const cv::Rect &a, const cv::Rect &b)
{
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x + a.width, b.x + b.width);
    int y2 = std::min(a.y + a.height, b.y + b.height);
    int w = std::max(0, x2 - x1);
    int h = std::max(0, y2 - y1);
    int inter = w * h;
    int areaA = a.width * a.height;
    int areaB = b.width * b.height;
    int uni = areaA + areaB - inter;
    return uni > 0 ? (float)inter / (float)uni : 0.f;
}