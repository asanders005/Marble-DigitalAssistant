from ultralytics import YOLO
model = YOLO('build/Assets/onnx/model.pt')   # or path to your .pt
model.export(format='onnx', dynamic=False, opset=13, simplify=True, imgsz=640)
print("ONNX model exported successfully.")