import cv2
import numpy as np

def convert_rgb565_to_rgb888(rgb565_data):
    # RGB565: 5 bits for red, 6 bits for green, 5 bits for blue
    r = (rgb565_data & 0xF800) >> 8
    g = (rgb565_data & 0x07E0) >> 3
    b = (rgb565_data & 0x001F) << 3
    return np.stack((r, g, b), axis=-1)

# Load raw image data
image_data = np.fromfile('captured_image.raw', dtype=np.uint16).reshape((480, 640))

# Convert RGB565 to RGB888
image_rgb888 = convert_rgb565_to_rgb888(image_data)

# Save the image as BMP
cv2.imwrite('captured_image.bmp', image_rgb888)

