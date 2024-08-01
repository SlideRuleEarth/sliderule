import torch
import torchdynamo
import importlib
import sys

def compile_and_save_model(model_path, save_path, num_part):
    # Load the model
    sys.path.append('/pointnet2')
    MODEL = importlib.import_module('pointnet2_part_seg_msg')

    device = 'cuda' if torch.cuda.is_available() else 'cpu'

    classifier = MODEL.get_model(num_part, conf_channel=True).to(device)
    trained_model = torch.load(model_path, map_location=torch.device(device))
    model_state_dict = {k.replace('module.', ''): v for k, v in trained_model['model_state_dict'].items()}
    classifier.load_state_dict(model_state_dict)

    backends = torch._dynamo.list_backends()
    print(f"Available backends: {backends}")

    # Compile the model with TorchDynamo for CPU target
    backend_optimization = 'inductor'
    # backend_optimization = 'tvm'
    # backend_optimization = 'onnxrt'
    classifier = torchdynamo.optimize(backend_optimization)(classifier)

    # Save the compiled model
    torch.save(classifier.state_dict(), save_path)
    print(f"Compiled model saved to {save_path}, used backend: {backend_optimization}")

if __name__ == "__main__":
    model_path = '/data/pointnet2_model.pth'
    save_path  = '/data/inductor_pointnet2_model.pth'
    num_part   = 2   # Set based on the runner.py script
    compile_and_save_model(model_path, save_path, num_part)
