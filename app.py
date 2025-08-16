import streamlit as st
import cv2
import tempfile
import torch
from PIL import Image
import numpy as np
from ultralytics import YOLO
from transformers import BlipProcessor, BlipForConditionalGeneration

# Streamlit config
st.set_page_config(page_title="ESP32-CAM + YOLOv8 + BLIP", layout="wide")
st.title("ESP32-CAM Object Detection & Image Captioning")

# Sidebar controls
st.sidebar.header("ESP32-CAM MJPEG Stream URL")
stream_url = st.sidebar.text_input("Stream URL", "http://192.168.0.102/stream")
run_yolo = st.sidebar.checkbox("🔴 Run YOLOv8 Detection", value=True)
run_blip = st.sidebar.checkbox("🔴 Run BLIP Captioning", value=True)
keep_running = st.sidebar.checkbox("📷 Keep Running", value=True)

# Load YOLOv8 model
@st.cache_resource
def load_yolo():
    return YOLO("yolov8n.pt")

# Load BLIP model
@st.cache_resource
def load_blip():
    processor = BlipProcessor.from_pretrained("Salesforce/blip-image-captioning-base")
    model = BlipForConditionalGeneration.from_pretrained("Salesforce/blip-image-captioning-base")
    return processor, model

# Detect objects
def detect_objects_yolo(frame, model):
    results = model(frame)
    return results[0].plot()

# Generate caption using BLIP
def generate_blip_caption(image, processor, model):
    inputs = processor(images=image, return_tensors="pt")
    output = model.generate(**inputs)
    caption = processor.decode(output[0], skip_special_tokens=True)
    return caption

# Main logic
def main():
    stframe = st.empty()
    caption_placeholder = st.empty()

    cap = cv2.VideoCapture(stream_url)

    if not cap.isOpened():
        st.error(" Failed to connect to ESP32-CAM. Check your URL or network.")
        return

    yolo_model = load_yolo() if run_yolo else None
    blip_processor, blip_model = load_blip() if run_blip else (None, None)

    while keep_running:
        ret, frame = cap.read()

        if not ret or frame is None:
            st.warning("Waiting for valid video frame...")
            continue

        # Rotate and crop if needed
        frame = cv2.rotate(frame, cv2.ROTATE_90_CLOCKWISE)
        h, w, _ = frame.shape
        frame = frame[:int(h * 0.93), :]

        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        pil_image = Image.fromarray(frame_rgb)

        if run_yolo:
            try:
                frame = detect_objects_yolo(frame_rgb, yolo_model)
            except Exception as e:
                st.error(f"YOLOv8 Detection Error: {e}")

        if run_blip:
            try:
                caption = generate_blip_caption(pil_image, blip_processor, blip_model)
                caption_placeholder.markdown(f"### Description: `{caption}`")
            except Exception as e:
                st.error(f"BLIP Captioning Error: {e}")
                
        desired_width = 800
        desired_height = 600
        resized_frame = cv2.resize(frame, (desired_width, desired_height))
        stframe.image(resized_frame, channels="RGB")

    cap.release()
    cv2.destroyAllWindows()

# Run app
if __name__ == "__main__":
    main()
