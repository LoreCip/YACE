import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim

# 1. Definizione dello Straight-Through Estimator (STE)
class FakeQuantizeSTE(torch.autograd.Function):
    @staticmethod
    def forward(ctx, x, scale, min_val, max_val):
        # Forward: scaliamo, arrotondiamo, limitiamo e de-scaliamo
        x_quant = torch.clamp(torch.round(x * scale), min_val, max_val)
        return x_quant / scale

    @staticmethod
    def backward(ctx, grad_output):
        # Backward: il gradiente passa intatto (STE)
        return grad_output, None, None, None

def fake_quant(x, scale, min_val, max_val):
    return FakeQuantizeSTE.apply(x, scale, min_val, max_val)

class HalfKP_NNUE_QAT(nn.Module):
    def __init__(self, acc_size=256, layer_1=32, layer_2=32):
        super(HalfKP_NNUE_QAT, self).__init__()
        
        self.accumulator = nn.Embedding(40961, acc_size, padding_idx=40960)
        self.accumulator_bias = nn.Parameter(torch.zeros(acc_size)) 
        
        self.layer1 = nn.Linear(acc_size * 2, layer_1)
        self.output_layer = nn.Linear(layer_1, layer_2)

        self.layer2 = nn.Linear(layer_1, layer_2)
        self.output_layer = nn.Linear(layer_2, 1)

        # --- SCALE DI QUANTIZZAZIONE ---
        # Questi valori determinano le precisioni intere che userai in C++.
        self.SCALE_W_ACC = 255.0  # Pesi accumulatore (int16)
        self.SCALE_ACT   = 127.0  # Attivazioni ClippedReLU (int8)
        self.SCALE_W_L1  = 64.0   # Pesi Layer 1 (int8)
        self.SCALE_W_L2  = 64.0   # Pesi Layer 2 (int8)
        self.SCALE_W_OUT = 127.0  # Pesi Layer Output (int16)

    def clipped_relu(self, x):
        return torch.clamp(x, 0.0, 1.0)

    def forward(self, us_indices, them_indices):
        # -- FAKE QUANTIZATION: ACCUMULATORE (Pesi in int16: -32768 a 32767) --
        acc_w_q = fake_quant(self.accumulator.weight, self.SCALE_W_ACC, -32768, 32767)
        acc_b_q = fake_quant(self.accumulator_bias, self.SCALE_W_ACC, -32768, 32767)
        
        acc_us = F.embedding(us_indices, acc_w_q, padding_idx=40960).sum(dim=1) + acc_b_q
        acc_them = F.embedding(them_indices, acc_w_q, padding_idx=40960).sum(dim=1) + acc_b_q
        
        acc_us = self.clipped_relu(acc_us)
        acc_them = self.clipped_relu(acc_them)        
        x = torch.cat([acc_us, acc_them], dim=1)
        x = fake_quant(x, self.SCALE_ACT, 0, 127)
        
        # -- FAKE QUANTIZATION: LAYER 1 (Pesi in int8: -128 a 127) --
        l1_w_q = fake_quant(self.layer1.weight, self.SCALE_W_L1, -128, 127)
        l1_b_q = fake_quant(self.layer1.bias, self.SCALE_W_ACC * self.SCALE_W_L1, -32768, 32767)
        
        x = F.linear(x, l1_w_q, l1_b_q)
        x = self.clipped_relu(x)
        x = fake_quant(x, self.SCALE_ACT, 0, 127)
        
        # -- FAKE QUANTIZATION LAYER 2 (Pesi in int8: -128 a 127) --
        l2_w_q = fake_quant(self.layer2.weight, self.SCALE_W_L2, -128, 127)
        l2_b_q = fake_quant(self.layer2.bias, self.SCALE_ACT * self.SCALE_W_L2, -32768, 32767)
        
        x = F.linear(x, l2_w_q, l2_b_q)
        x = self.clipped_relu(x)
        x = fake_quant(x, self.SCALE_ACT, 0, 127)
        
        # -- FAKE QUANTIZATION: LAYER OUTPUT --
        out_w_q = fake_quant(self.output_layer.weight, self.SCALE_W_OUT, -32768, 32767)
        out_b_q = fake_quant(self.output_layer.bias, self.SCALE_ACT * self.SCALE_W_OUT, -32768, 32767)
        
        x = F.linear(x, out_w_q, out_b_q)
        
        return torch.sigmoid(x)