import torch
import os

def extract_pure_weights(checkpoint_path, output_path):
    if not os.path.exists(checkpoint_path):
        print(f"❌ Errore: Il file di origine '{checkpoint_path}' non esiste.")
        return

    print(f"Caricamento del checkpoint completo: {checkpoint_path}")
    checkpoint = torch.load(checkpoint_path, map_location="cpu", weights_only=False)

    if isinstance(checkpoint, dict) and "model_state_dict" in checkpoint:
        print("-> Rilevato checkpoint completo dell'addestramento.")
        state_dict = checkpoint["model_state_dict"]
        
        epoch = checkpoint.get("epoch", "N/D")
        global_step = checkpoint.get("global_step", "N/D")
        loss = checkpoint.get("loss", "N/D")
        print(f"   Dati estratti dal salvataggio: Epoca {epoch} | Step {global_step} | Ultima Loss: {loss:.6f}")
    else:
        print("-> Il file contiene già i pesi puri (o una struttura non standard).")
        state_dict = checkpoint

    clean_state_dict = {}
    compiled_detected = False
    
    for key, value in state_dict.items():
        if key.startswith("_orig_mod."):
            new_key = key.replace("_orig_mod.", "")
            compiled_detected = True
        else:
            new_key = key
        clean_state_dict[new_key] = value

    if compiled_detected:
        print("-> Rimosso il prefisso '_orig_mod.' generato da torch.compile.")

    torch.save(clean_state_dict, output_path)
    
    old_size = os.path.path.getsize(checkpoint_path) / (1024 * 1024)
    new_size = os.path.path.getsize(output_path) / (1024 * 1024)
    
    print(f"\n Operazione completata con successo!")
    print(f"   File originale (Full): {old_size:.2f} MB")
    print(f"   File finale (Pesi Puri): {new_size:.2f} MB (Riduzione del ~{((old_size-new_size)/old_size)*100:.1f}%)")
    print(f"   Salvato in: {output_path}")

if __name__ == "__main__":
    CHECKPOINT_COMPLETO = "checkpoints/halfkp_epoch_15_full_checkpoint.pth"
    SOLO_PESI = "checkpoints/halfkp_epoch_15_weights_puliti.pth"
    
    extract_pure_weights(CHECKPOINT_COMPLETO, SOLO_PESI)