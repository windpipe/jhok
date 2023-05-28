#pragma once
#ifndef _CAMERAMANAGER_H_
#define _CAMERAMANAGER_H_
#include "Camera.h"
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <opencv2/opencv.hpp>


class CameraManager {
public:
    void addCamera(int index, int monitor) {
        cameras.emplace_back(std::make_unique<Camera>(index, monitor));
    }

    void startCapture() {
        for (auto& camera : cameras) {
            camera->startCapture();
        }
    }

    void stopCapture() {
        for (auto& camera : cameras) {
            camera->stopCapture();
        }
    }



    int getNumCameras() {
        std::lock_guard<std::mutex> lock(cameras_mutex); // Lock the mutex
        return cameras.size();
    }


    void getFrame(int index, cv::Mat& frame) {
        cameras[index]->getFrame(frame);
    }
    GLuint getTexture(int index) {
        return cameras[index]->getTexture();
    }


    Camera& getCamera(int index) {
        return *cameras[index];
    }

    std::pair<int, int> getCameraTextureSize(int index) {
        if (index < 0 || index >= cameras.size()) {
            throw std::out_of_range("Camera index out of range");
        }

        return cameras[index]->getTextureSize();
    }
    void deleteAllTextures() {
        for (auto& camera : cameras) {
            camera->deleteTexture();
        }
    }
    // Add these methods to your CameraManager class
    void CameraManager::setSelectedCamera(int index) {
        selectedCameraIndex = index;
    }

    int CameraManager::getSelectedCameraIndex() {
        return selectedCameraIndex;
    }
private:
    int selectedCameraIndex = 0;
    std::vector<std::unique_ptr<Camera>> cameras;
    mutable std::mutex cameras_mutex; // Mutex to protect access to cameras
};



#endif