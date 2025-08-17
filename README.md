# Real-Time-Object-Detection-and-Visual-Captioning-Using-ESP32-CAM-and-Deep-Learning-Models

PROPOSED ALGORITHM

The following algorithm outlines the steps followed in the
real-time object detection and captioning system using ESP32-
CAM, YOLOv8, and BLIP.

1: Start

2: Initialize the ESP32-CAM stream URL

3: Load YOLOv8 detection model and BLIP captioning
model

4: while stream is active do

5: Capture frame from ESP32-CAM

6: if YOLO detection is enabled then

7: Apply YOLOv8 model on frame

8: Display detected objects on screen

9: end if

10: if Captioning is enabled then

11: Convert frame to RGB

12: Generate caption using BLIP model

13: Display caption

14: end if

15: end while

16: Release the stream and models

17: End

[project.webm](https://github.com/user-attachments/assets/b31223e2-6870-41a1-896c-ea7889355188)

Hardware Used:


<img width="500" height="500" alt="image" src="https://github.com/user-attachments/assets/9d364a72-7fa3-4f99-85be-979d1abfb17b" />

System Architecture:

<img width="422" height="140" alt="image" src="https://github.com/user-attachments/assets/0fb8b77f-8b90-41dc-90ae-040c1ca8cb22" />





