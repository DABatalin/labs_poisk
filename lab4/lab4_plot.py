import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def main():
    print("Loading CSV data...")
    df = pd.read_csv("zipf_data.csv")
    
    df_head = df.head(10000).copy()
    
    ranks = df_head['rank'].values
    freqs = df_head['frequency'].values
    
    C = freqs[0]
    zipf_ideal = [C / r for r in ranks]

    alpha = 1.05
    beta = 10.0
    P = freqs[0] * ((1 + beta) ** alpha)
    
    mandelbrot_ideal = [P / ((r + beta) ** alpha) for r in ranks]

    plt.figure(figsize=(12, 8))
    
    plt.loglog(ranks, freqs, label='Real Data (Distribution)', linewidth=2, color='blue', alpha=0.7)
    plt.loglog(ranks, zipf_ideal, label=f'Zipf\'s Law (Ideal, slope=-1)', linestyle='--', color='red')
    plt.loglog(ranks, mandelbrot_ideal, label=f'Mandelbrot (alpha={alpha}, beta={beta})', linestyle='-.', color='green')

    plt.title("Zipf's Law Analysis: Information Technology Corpus")
    plt.xlabel("Rank (Log scale)")
    plt.ylabel("Frequency (Log scale)")
    plt.grid(True, which="both", ls="-", alpha=0.2)
    plt.legend()
    
    output_img = "lab4_zipf_plot.png"
    plt.savefig(output_img)
    plt.show()
    print(f"Graph saved to {output_img}")
    
    print("\n--- Statistics for Report ---")
    print(f"Most frequent word (Rank 1): {df['word'].iloc[0]} ({df['frequency'].iloc[0]})")
    print(f"Word at Rank 10: {df['word'].iloc[9]} ({df['frequency'].iloc[9]})")
    print(f"Word at Rank 100: {df['word'].iloc[99]} ({df['frequency'].iloc[99]})")
    
    single_use_count = len(df[df['frequency'] == 1])
    print(f"Words appearing only once (Hapax Legomena): {single_use_count}")
    print(f"Percentage of dictionary: {single_use_count / len(df) * 100:.2f}%")

if __name__ == "__main__":
    main()
