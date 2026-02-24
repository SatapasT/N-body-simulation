import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

# Updated data (seconds)
data = {
    "Scenario": (
        ["scenario1_stable"] * 7 +
        ["scenario2_unstable"] * 7 +
        ["N=512"] * 7 +
        ["N=1000"] * 7
    ),
    "Threads": (
        [1, 1, 1, 2, 4, 8, 16] * 4
    ),
    "Variant": (
        ["Serial (no vectorisation)", "Serial (vectorised)",
         "Parallel+vectorised", "Parallel+vectorised", "Parallel+vectorised", "Parallel+vectorised", "Parallel+vectorised"] * 4
    ),
    "Time (s)": [
        # scenario1_stable
        0.15, 0.16, 0.18, 0.21, 0.22, 0.23, 0.27,
        # scenario2_unstable
        0.16, 0.16, 0.20, 0.23, 0.23, 0.24, 0.27,
        # N=512
        11.35, 11.37, 15.30, 11.14, 8.88, 7.82, 7.29,
        # N=1000
        36.33, 36.40, 46.58, 30.00, 21.38, 16.97, 15.05
    ]
}

df = pd.DataFrame(data)

sns.set(style="whitegrid", context="talk")

for scenario in df["Scenario"].unique():
    plt.figure(figsize=(10, 6))
    subset = df[df["Scenario"] == scenario]

    # Keep ordering consistent
    subset = subset.copy()
    subset["Threads"] = pd.Categorical(subset["Threads"], categories=[1, 2, 4, 8, 16], ordered=True)

    sns.barplot(
        data=subset,
        x="Threads",
        y="Time (s)",
        hue="Variant"
    )

    plt.title(f"Runtime Scaling - {scenario}")
    plt.tight_layout()
    plt.savefig(f"{scenario}_barchart.png", dpi=300)
    plt.close()

print("Plots saved successfully.")