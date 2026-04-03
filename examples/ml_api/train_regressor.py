#!/usr/bin/env python3
import math
import random
from pathlib import Path

from pytorch_dataset import VioSpiceJsonlDataset


try:
    import torch
    from torch import nn
    from torch.utils.data import DataLoader, random_split
except ImportError as exc:  # pragma: no cover
    raise SystemExit("torch is not installed. Install torch to run this example.") from exc


DATASET_PATH = "/tmp/viospice-datasets/amp.jsonl"
LABEL_NAMES = ["gain_ratio"]
STAT_FEATURES = [("ALL", "avg"), ("ALL", "max"), ("ALL", "rms")]
PARAM_FEATURES = ["vin_dc"]
SEED = 42
EPOCHS = 20
BATCH_SIZE = 32
LR = 1e-3


class Regressor(nn.Module):
    def __init__(self, input_dim: int, output_dim: int):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(input_dim, 32),
            nn.ReLU(),
            nn.Linear(32, 32),
            nn.ReLU(),
            nn.Linear(32, output_dim),
        )

    def forward(self, x):
        return self.net(x)


def split_dataset(dataset):
    total = len(dataset)
    train_size = max(1, int(total * 0.8))
    val_size = max(1, total - train_size)
    if train_size + val_size > total:
        train_size = total - 1
        val_size = 1
    generator = torch.Generator().manual_seed(SEED)
    return random_split(dataset, [train_size, val_size], generator=generator)


def evaluate(model, loader, loss_fn, device):
    model.eval()
    losses = []
    with torch.no_grad():
        for x, y in loader:
            x = x.to(device)
            y = y.to(device)
            pred = model(x)
            loss = loss_fn(pred, y)
            losses.append(float(loss.item()))
    return sum(losses) / max(1, len(losses))


def main():
    random.seed(SEED)
    torch.manual_seed(SEED)

    dataset_path = Path(DATASET_PATH)
    if not dataset_path.exists():
        raise SystemExit(f"dataset not found: {dataset_path}")

    dataset = VioSpiceJsonlDataset(
        str(dataset_path),
        label_names=LABEL_NAMES,
        stat_features=STAT_FEATURES,
        param_features=PARAM_FEATURES,
        accepted_only=True,
    )
    if len(dataset) < 2:
        raise SystemExit("dataset is too small for a train/validation split")

    train_ds, val_ds = split_dataset(dataset)
    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE, shuffle=False)

    sample_x, sample_y = dataset[0]
    model = Regressor(sample_x.shape[0], sample_y.shape[0])
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model.to(device)

    optimizer = torch.optim.Adam(model.parameters(), lr=LR)
    loss_fn = nn.MSELoss()

    for epoch in range(1, EPOCHS + 1):
        model.train()
        epoch_losses = []
        for x, y in train_loader:
            x = x.to(device)
            y = y.to(device)
            optimizer.zero_grad()
            pred = model(x)
            loss = loss_fn(pred, y)
            loss.backward()
            optimizer.step()
            epoch_losses.append(float(loss.item()))

        train_loss = sum(epoch_losses) / max(1, len(epoch_losses))
        val_loss = evaluate(model, val_loader, loss_fn, device)
        print(f"epoch={epoch:02d} train_loss={train_loss:.6f} val_loss={val_loss:.6f}")

    model.eval()
    with torch.no_grad():
        pred = model(sample_x.unsqueeze(0).to(device)).cpu().squeeze(0)
    print("sample target:", sample_y.tolist())
    print("sample prediction:", pred.tolist())


if __name__ == "__main__":
    main()
