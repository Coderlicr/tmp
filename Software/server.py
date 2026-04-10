import cv2
import socket
import threading
import queue

# Video file
frame_rate = 30  # fps
video_file = "big_buck_bunny_240p_30mb.mp4"

class Server:
    def __init__(self) -> None:
        # port
        self.server_port = 9999
        # socket
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.bind(("192.168.2.1", self.server_port))
        self.server_socket.listen(10)
        self.client_socket_list = []
        self.frame_queue = queue.Queue(maxsize=10)
        print("Server running...")

    def accept_client(self):
        while True:
            client_socket, client_address = self.server_socket.accept()
            print(f"[*] Accepted connection from {client_address}")
            self.client_socket_list.append((client_socket, client_address[0]))

    def stream_video(self, video_file, frame_rate):
        video_capture = cv2.VideoCapture(video_file)
        if not video_capture.isOpened():
            print("Error: Could not open video file.")
            return

        while True:
            ret, frame = video_capture.read()
            if not ret:
                print("Failed to read frame; breaking loop...")
                break

            try:
                # serialize the frame
                serialized_frame = cv2.imencode(".jpg", frame)[1].tobytes()
                self.frame_queue.put(frame)  # Put the frame in the queue
                for client_socket, client_address in self.client_socket_list:
                    try:
                        client_socket.sendall(serialized_frame)
                    except Exception as e:
                        print(f"Error sending frame to {client_address}: {str(e)}")
                        self.client_socket_list.remove((client_socket, client_address))
                        client_socket.close()
            except Exception as e:
                print(f"Failed to encode or send frame: {str(e)}")

        video_capture.release()
        self.server_socket.close()

    def __del__(self):
        self.server_socket.close()

def display_video(frame_rate, frame_queue):
    while True:
        if not frame_queue.empty():
            frame = frame_queue.get()
            if frame is not None:
                cv2.imshow("Server Video", frame)
                if cv2.waitKey(int(1000 / frame_rate)) & 0xFF == ord('q'):
                    break
            else:
                print("Received an empty frame.")
    cv2.destroyAllWindows()

my_server = Server()
# launch threads
thread1 = threading.Thread(target=my_server.accept_client)
thread2 = threading.Thread(target=my_server.stream_video, args=(video_file, frame_rate))

thread1.start()
thread2.start()

# Run GUI in the main thread
display_video(frame_rate, my_server.frame_queue)
