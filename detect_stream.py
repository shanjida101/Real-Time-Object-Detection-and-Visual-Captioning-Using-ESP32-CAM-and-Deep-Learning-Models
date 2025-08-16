from ultralytics import YOLO
import cv2

model = YOLO("yolov8n.pt")

def detect_objects(frame):
    results = model.predict(source=frame, conf=0.3, verbose=False)
    return results[0].plot()
