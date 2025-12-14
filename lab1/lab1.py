import requests
from bs4 import BeautifulSoup
import os
import time

URLS_TO_ANALYZE = [
    "https://www.opennet.ru/opennews/art.shtml?num=60000",
    "https://www.opennet.ru/opennews/art.shtml?num=60500",
    "https://www.opennet.ru/opennews/art.shtml?num=61000",
    
    "https://habr.com/ru/articles/780000/",
    "https://habr.com/ru/articles/775000/", 
    "https://habr.com/ru/articles/790000/" 
]

OUTPUT_DIR = "lab1_data"
STATS = {
    "habr":    {"count": 0, "raw": 0, "text": 0},
    "opennet": {"count": 0, "raw": 0, "text": 0}
}

def ensure_dir(directory):
    if not os.path.exists(directory):
        os.makedirs(directory)

def fetch_and_process(url, index):
    try:
        headers = {
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8'
        }
        
        response = requests.get(url, headers=headers, timeout=10)

        response.raise_for_status()
        
        raw_html = response.content 
        raw_size = len(raw_html)
        
        soup = BeautifulSoup(response.text, 'html.parser')
        
        for tag in soup(["script", "style", "meta", "noscript", "iframe", "svg"]):
            tag.extract()
            
        text = soup.get_text(separator=' ')
        
        lines = (line.strip() for line in text.splitlines())
        chunks = (phrase.strip() for line in lines for phrase in line.split("  "))
        clean_text = '\n'.join(chunk for chunk in chunks if chunk)
        
        clean_text_bytes = clean_text.encode('utf-8')
        clean_size = len(clean_text_bytes)

        source_prefix = "opennet" if "opennet.ru" in url else "habr"
        filename_base = f"{source_prefix}_{index}"
        
        with open(os.path.join(OUTPUT_DIR, f"{filename_base}_raw.html"), "wb") as f:
            f.write(raw_html)
            
        with open(os.path.join(OUTPUT_DIR, f"{filename_base}_clean.txt"), "wb") as f:
            f.write(clean_text_bytes)

        print(f"[{index}] {url}")
        print(f"    Raw size:  {raw_size} bytes")
        print(f"    Text size: {clean_size} bytes")
        
        source_key = "habr" if "habr.com" in url else "opennet"
    
        STATS[source_key]["count"] += 1
        STATS[source_key]["raw"] += raw_size
        STATS[source_key]["text"] += clean_size

    except Exception as e:
        print(f"Error processing {url}: {e}")


ensure_dir(OUTPUT_DIR)

for i, url in enumerate(URLS_TO_ANALYZE):
    fetch_and_process(url, i)
    time.sleep(1.5) 
print("\nСтатистика ")
print(f"{'Source':<10} | {'Docs':<5} | {'Avg Raw (KB)':<15} | {'Avg Text (KB)':<15} | {'Ratio (%)':<10}")
print("-" * 65)

for source, data in STATS.items():
    if data["count"] > 0:
        avg_raw = data["raw"] / data["count"]
        avg_text = data["text"] / data["count"]
        ratio = (avg_text / avg_raw) * 100
        
        print(f"{source:<10} | {data['count']:<5} | {avg_raw/1024:<15.2f} | {avg_text/1024:<15.2f} | {ratio:<10.2f}")

total_count = sum(d["count"] for d in STATS.values())
if total_count > 0:
    total_raw = sum(d["raw"] for d in STATS.values())
    total_text = sum(d["text"] for d in STATS.values())
    print("-" * 65)
    print(f"TOTAL      | {total_count:<5} | {total_raw/total_count/1024:<15.2f} | {total_text/total_count/1024:<15.2f} | {(total_text/total_raw)*100:<10.2f}")