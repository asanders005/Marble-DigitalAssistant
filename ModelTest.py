import onnxruntime as ort
import numpy as np
import cv2

# Load image and preprocess (resize, BGR to RGB, normalize)
img = cv2.imread('build/Assets/Images/PersonTest.jpg')
img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
img_resized = cv2.resize(img, (640, 640))
img_input = img_resized.astype(np.float32) / 255.0
img_input = np.transpose(img_input, (2, 0, 1))  # HWC to CHW
img_input = np.expand_dims(img_input, axis=0)   # Add batch dim

# Load ONNX model
session = ort.InferenceSession('build/Assets/onnx/yolov5n-sim.onnx', providers=['CPUExecutionProvider'])
input_name = session.get_inputs()[0].name

# Run inference
outputs = session.run(None, {input_name: img_input})

# Print output shape and top class for each detection
output = outputs[0]  # shape: (1, attrs, num_preds)
print('Output shape:', output.shape)
attrs, num_preds = output.shape[1], output.shape[2]
for i in range(min(10, num_preds)):
    row = output[0, i, :]
    obj_conf = 1 / (1 + np.exp(-row[4]))
    class_scores = 1 / (1 + np.exp(-row[5:]))  # sigmoid
    class_confs = obj_conf * class_scores
    best_class = np.argmax(class_confs)
    best_conf = class_confs[best_class]
    print(f'Pred {i}: conf={best_conf:.3f}, class={best_class}')