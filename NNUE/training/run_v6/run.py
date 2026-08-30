import sys
sys.path.insert(0, "/home/lorenzo/Scrivania/Projects/YACE/NNUE/training")
from trainingNNUE_HalfKP import train_nnue

train_nnue(
    bin_path="/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_halfkp_balanced.bin",
    num_epochs=40,
    batch_size=16384,
    learning_rate=0.001,
    resume_from=None,
)
