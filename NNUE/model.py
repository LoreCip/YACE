"""Rete HalfKP con quantization-aware training, specchio di NnueNetwork in C++:
    accumulatore 40960 → 256 per prospettiva (chi muove, avversario) → ClippedReLU
    → concat 512 → L1 32 → ClippedReLU → L2 32 → ClippedReLU → output (logit, cp = logit*400)
Le scale sotto sono quelle usate da training/export_weights.py per scrivere il .bin del motore.
"""
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

from halfkp import CP_SCALE, NUM_FEATURES, PAD_IDX

# Scale di quantizzazione: le stesse usate da training/export_weights.py per l'export verso il C++.
# Nota: l'export quantizza L0 a SCALE_ACT (127), il training simula SCALE_W_ACC (255). Scarto
# misurato tra rete PyTorch e motore C++: ~2.5 cp di MAE, trascurabile rispetto all'errore della rete.
SCALE_W_ACC = 255.0  # Pesi accumulatore (int16) durante il training
SCALE_ACT   = 127.0  # Attivazioni ClippedReLU (int8)
SCALE_W_L1  = 64.0   # Pesi Layer 1 (int8)
SCALE_W_L2  = 64.0   # Pesi Layer 2 (int8)
SCALE_W_OUT = 127.0  # Pesi Layer Output (int16)

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
        
        self.accumulator = nn.Embedding(NUM_FEATURES + 1, acc_size, padding_idx=PAD_IDX)
        self.accumulator_bias = nn.Parameter(torch.zeros(acc_size)) 
        
        self.layer1 = nn.Linear(acc_size * 2, layer_1)
        self.layer2 = nn.Linear(layer_1, layer_2)
        self.output_layer = nn.Linear(layer_2, 1)

    def clipped_relu(self, x):
        return torch.clamp(x, 0.0, 1.0)

    def forward(self, us_indices, them_indices, return_logits=False):
        # -- FAKE QUANTIZATION: ACCUMULATORE (Pesi in int16: -32768 a 32767) --
        acc_w_q = fake_quant(self.accumulator.weight, SCALE_W_ACC, -32768, 32767)
        acc_b_q = fake_quant(self.accumulator_bias, SCALE_W_ACC, -32768, 32767)
        
        acc_us = F.embedding(us_indices, acc_w_q, padding_idx=PAD_IDX).sum(dim=1) + acc_b_q
        acc_them = F.embedding(them_indices, acc_w_q, padding_idx=PAD_IDX).sum(dim=1) + acc_b_q
        
        acc_us = self.clipped_relu(acc_us)
        acc_them = self.clipped_relu(acc_them)        
        x = torch.cat([acc_us, acc_them], dim=1)
        x = fake_quant(x, SCALE_ACT, 0, 127)
        
        # -- FAKE QUANTIZATION: LAYER 1 (Pesi in int8: -128 a 127) --
        l1_w_q = fake_quant(self.layer1.weight, SCALE_W_L1, -128, 127)
        l1_b_q = fake_quant(self.layer1.bias, SCALE_W_ACC * SCALE_W_L1, -32768, 32767)
        
        x = F.linear(x, l1_w_q, l1_b_q)
        x = self.clipped_relu(x)
        x = fake_quant(x, SCALE_ACT, 0, 127)
        
        # -- FAKE QUANTIZATION LAYER 2 (Pesi in int8: -128 a 127) --
        l2_w_q = fake_quant(self.layer2.weight, SCALE_W_L2, -128, 127)
        l2_b_q = fake_quant(self.layer2.bias, SCALE_ACT * SCALE_W_L2, -32768, 32767)
        
        x = F.linear(x, l2_w_q, l2_b_q)
        x = self.clipped_relu(x)
        x = fake_quant(x, SCALE_ACT, 0, 127)
        
        # -- FAKE QUANTIZATION: LAYER OUTPUT --
        out_w_q = fake_quant(self.output_layer.weight, SCALE_W_OUT, -32768, 32767)
        out_b_q = fake_quant(self.output_layer.bias, SCALE_ACT * SCALE_W_OUT, -32768, 32767)
        
        x = F.linear(x, out_w_q, out_b_q)

        if return_logits:
            return torch.sigmoid(x), x
        return torch.sigmoid(x)


def logit(p, eps=1e-6):
    """Inverso della sigmoide, con clamp per evitare infiniti."""
    p = torch.clamp(p, eps, 1.0 - eps)
    return torch.log(p / (1.0 - p))


def prob_to_cp(p):
    """Probabilita' di vittoria (target o output della rete) → centipawn."""
    return logit(p) * CP_SCALE


def load_model(path, device="cpu"):
    """Carica un checkpoint di training (anche salvato da torch.compile) in modalita' eval."""
    checkpoint = torch.load(path, map_location=device, weights_only=False)
    state_dict = checkpoint.get("model_state_dict", checkpoint)
    model = HalfKP_NNUE_QAT().to(device)
    model.load_state_dict({k.replace("_orig_mod.", ""): v for k, v in state_dict.items()})
    model.eval()
    return model


def to_tensors(rows, device):
    """Righe del dataset (DATASET_DTYPE) → tensori (us, them, target) sul device."""
    us = torch.from_numpy(rows["us"].astype(np.int64)).to(device)
    them = torch.from_numpy(rows["them"].astype(np.int64)).to(device)
    target = torch.from_numpy(rows["target"].astype(np.float32)).to(device)
    return us, them, target
