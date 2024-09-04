# Copyright (c) 2021, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import torch
import torchdynamo
import importlib
import sys

def compile_and_save_model(model_path, save_path, num_part):
    # Load the model
    sys.path.append('/pointnet')
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
