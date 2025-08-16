from ultralytics import YOLO
import cv2, requests, numpy as np, time, argparse

def open_mjpeg(base, tries=3):
    """Try /stream (port 80) then :81/stream, a few times."""
    candidates = [f"{base}/stream", f"{base}:81/stream"]
    for t in range(tries):
        for url in candidates:
            cap = cv2.VideoCapture(url, cv2.CAP_FFMPEG)
            if cap.isOpened():
                print(f"[i] Using MJPEG: {url}")
                return cap, url
            cap.release()
        print(f"[i] MJPEG open failed (try {t+1}/{tries}); retrying in 1s...")
        time.sleep(1)
    return None, None

def snapshot_grabber(snap_url):
    """Return a closure that grabs JPEG snapshots with keep-alive session."""
    sess = requests.Session()
    sess.headers.update({"Connection": "keep-alive"})
    def grab():
        try:
            img = sess.get(snap_url, timeout=2).content
            return cv2.imdecode(np.frombuffer(img, np.uint8), cv2.IMREAD_COLOR)
        except Exception:
            return None
    return grab

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True, help="ESP32-CAM base URL, e.g., http://192.168.0.193")
    ap.add_argument("--weights", default="yolov8n.pt", help="YOLO weights (or your best.pt)")
    ap.add_argument("--conf", type=float, default=0.35)
    ap.add_argument("--imgsz", type=int, default=320, help="YOLO inference size (smaller=faster)")
    args = ap.parse_args()

    base = args.ip.rstrip("/")
    snap_url = f"{base}/capture"

    print(f"[i] Loading model: {args.weights}")
    model = YOLO(args.weights)

    cap, current_url = open_mjpeg(base)
    use_snapshot = cap is None
    grab = snapshot_grabber(snap_url)

    last_ok = time.time()
    while True:
        if use_snapshot:
            frame = grab()
            if frame is None:
                continue
            time.sleep(0.03)   # ~30 FPS cap
        else:
            ok, frame = cap.read()
            if not ok:
                print("[!] MJPEG read failed; attempting reconnect...")
                # release and try to reopen MJPEG
                cap.release()
                cap, current_url = open_mjpeg(base, tries=2)
                if cap is None:
                    print("[i] Falling back to /capture polling.")
                    use_snapshot = True
                    continue

        # Inference (smaller imgsz = faster)
        results = model.predict(source=frame, imgsz=args.imgsz, conf=args.conf, verbose=False)[0]
        annotated = results.plot()
        cv2.imshow("ESP32-CAM YOLO (ESC to quit)", annotated)
        if cv2.waitKey(1) & 0xFF == 27:
            break

    try:
        if cap: cap.release()
    except: pass
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
