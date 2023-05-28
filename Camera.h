#pragma once
#ifndef _CAMERA_H_
#define _CAMERA_H_
#include <opencv2/opencv.hpp>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>

using namespace std;


class Camera {
public:
    Camera(int index, int monitor) : index(index), monitor(monitor) {
        cap = cv::VideoCapture(index, cv::CAP_DSHOW);
        setResolution(1920, 1080);
        // Record the actual resolution
        actualWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        actualHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    }

    void setResolution(int width, int height) {
        std::unique_lock<std::mutex> lock(frame_mutex);
        if (capture_thread.joinable()) {
            capture_running = false;
            capture_thread.join();
        }

        cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);

        // Restart capture if it was running
        if (capture_running) {
            startCapture();
        }
    }

    void startCapture() {
        capture_running = true;
        capture_thread = std::thread(&Camera::capture_frames, this);
    }

    void stopCapture() {
        capture_running = false;
        if (capture_thread.joinable()) {
            capture_thread.join();
        }
    }

    GLuint getTexture() {
        std::unique_lock<std::mutex> lock(frame_mutex);
        if (frameChanged) {
            if (texture != 0) {
                glDeleteTextures(1, &texture); // Delete the old texture
            }
            texture = createTexture(frame); // Create a new texture
            frameChanged = false;
        }
        return texture;
    }

    std::pair<int, int> getTextureSize() {
        std::unique_lock<std::mutex> lock(frame_mutex);
        return { actualWidth, actualHeight };
    }
    void getFrame(cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(frame_mutex);
        
        frame = this->frame.clone();
    }
    void deleteTexture() {
        if (texture != 0) {
			glDeleteTextures(1, &texture);
			texture = 0;
		}
	}
private:
    int index;
    int monitor;
    cv::VideoCapture cap;
    cv::Mat frame;
    std::mutex frame_mutex;
    std::thread capture_thread;
    std::atomic<bool> capture_running;
    int actualWidth, actualHeight;  // Actual resolution
    GLuint texture = 0;
    bool frameChanged = false;

    void capture_frames() {
        cv::Mat tmp_frame;
        while (capture_running) {
            cap >> tmp_frame;

            if (tmp_frame.empty()) {
                break;
            }
            cv::cvtColor(tmp_frame, tmp_frame, cv::COLOR_BGR2RGB);  // Convert from BGR to RGB

            {
                std::unique_lock<std::mutex> lock(frame_mutex);
                frame = tmp_frame.clone();
                frameChanged = true;
            }
        }
    }

    GLuint createTexture(cv::Mat& frame) {
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame.cols, frame.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, frame.data);

        glBindTexture(GL_TEXTURE_2D, 0); // Unbind

        return textureID;
    }
};


#endif