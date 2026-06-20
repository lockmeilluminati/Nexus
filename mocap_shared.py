import cv2
import mediapipe as mp
from mediapipe.tasks import python
from mediapipe.tasks.python import vision
import struct
import mmap
import sys
import traceback
import os
import urllib.request

def download_model():
    model_path = 'pose_landmarker_heavy.task'
    model_url = 'https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_heavy/float16/1/pose_landmarker_heavy.task'
    
    if not os.path.exists(model_path):
        print(f"\n[!] AI Model missing. Downloading from Google ({model_path})...")
        print("This might take a minute depending on your internet speed...")
        urllib.request.urlretrieve(model_url, model_path)
        print("[!] Download Complete!\n")

def main():
    # THE FIX: Auto-Download the model if it's missing!
    download_model()

    print("[1] Allocating Shared Memory...")
    try:
        shmem = mmap.mmap(-1, 256, "NexusMocapMemory")
    except Exception as e:
        print(f"Failed to open memory. Start C++ Engine first! Error: {e}")
        return

    print("[2] Memory Locked! Booting MediaPipe HEAVY...")

    base_options = python.BaseOptions(model_asset_path='pose_landmarker_heavy.task')
    
    options = vision.PoseLandmarkerOptions(
        base_options=base_options,
        running_mode=vision.RunningMode.IMAGE, 
        min_pose_detection_confidence=0.5)

    detector = vision.PoseLandmarker.create_from_options(options)
    cap = cv2.VideoCapture(0)

    # 14 points mapped for the C++ Engine 
    CONNECTIONS = [
        (11, 12), (11, 23), (12, 24), (23, 24), 
        (11, 13), (13, 15), 
        (12, 14), (14, 16), 
        (23, 25), (25, 27), 
        (24, 26), (26, 28)  
    ]

    print("[3] Live! Sending data to Nexus Engine...")

    while cap.isOpened():
        success, frame = cap.read()
        if not success:
            break

        # Flip for mirror effect and convert BGR to RGB
        frame = cv2.flip(frame, 1)
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb_frame)
        
        detection_result = detector.detect(mp_image)

        if detection_result.pose_world_landmarks:
            wl = detection_result.pose_world_landmarks[0]
            pl = detection_result.pose_landmarks[0]
            
            # Pack exactly 39 floats. Y is inverted (-wl.y) to match Raylib 3D space!
            packet = struct.pack('39f', 
                wl[11].x, -wl[11].y, wl[11].z,  
                wl[12].x, -wl[12].y, wl[12].z,  
                wl[13].x, -wl[13].y, wl[13].z,  
                wl[14].x, -wl[14].y, wl[14].z,  
                wl[15].x, -wl[15].y, wl[15].z,  
                wl[16].x, -wl[16].y, wl[16].z,  
                wl[23].x, -wl[23].y, wl[23].z,  
                wl[24].x, -wl[24].y, wl[24].z,  
                wl[25].x, -wl[25].y, wl[25].z,  
                wl[26].x, -wl[26].y, wl[26].z,  
                wl[27].x, -wl[27].y, wl[27].z,  
                wl[28].x, -wl[28].y, wl[28].z,  
                wl[0].x,  -wl[0].y,  wl[0].z    
            )
            
            shmem.seek(0)
            shmem.write(packet)

            h, w, c = frame.shape
            for connection in CONNECTIONS:
                cv2.line(frame, 
                         (int(pl[connection[0]].x * w), int(pl[connection[0]].y * h)), 
                         (int(pl[connection[1]].x * w), int(pl[connection[1]].y * h)), 
                         (0, 255, 0), 2)
            for idx in [0, 11, 12, 13, 14, 15, 16, 23, 24, 25, 26, 27, 28]:
                cv2.circle(frame, (int(pl[idx].x * w), int(pl[idx].y * h)), 5, (0, 0, 255), -1)

        cv2.imshow('Nexus Engine - AI MoCap Vision', frame)
        
        if cv2.waitKey(1) & 0xFF == 27: 
            break

    cap.release()
    cv2.destroyAllWindows()
    shmem.close()

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print("\n=== FATAL ERROR OCCURRED ===")
        traceback.print_exc()
        input("\nPress ENTER to close this window...")