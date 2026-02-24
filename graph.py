import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

# Data
data = {
    "Scenario": (
        ["scenario1_stable"] * 6 +
        ["scenario2_unstable"] * 6 +
        ["N=512"] * 6 +
        ["N=1000"] * 6
    ),
    "Threads": [1,1,2,4,8,16] * 4,
    "Variant": (
        ["Baseline","Full","Full","Full","Full","Full"] * 4
    ),
    "Time (s)": [
        0.16,0.19,0.30,0.24,0.24,0.27,
        0.17,0.20,0.24,0.24,0.26,0.27,
        11.54,15.58,11.39,9.06,7.89,7.34,
        36.28,47.29,30.17,21.53,17.33,15.12
    ]
}

df = pd.DataFrame(data)

sns.set(style="whitegrid", context="talk")

for scenario in df["Scenario"].unique():
    plt.figure(figsize=(10,6))
    subset = df[df["Scenario"] == scenario]
    
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