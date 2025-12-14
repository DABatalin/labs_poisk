from pymongo import MongoClient
from bs4 import BeautifulSoup
import re

client = MongoClient("localhost", 27017)
collection = client.search_engine_db.raw_docs

output_file = "../data/corpus_final.txt"

print("Начинаем экспорт корпуса...")

with open(output_file, "w", encoding="utf-8") as f:
    cursor = collection.find({})
    count = 0
    for doc in cursor:
        try:
            url = doc.get("url", "")
            raw_html = doc.get("raw_html", "")
            
            soup = BeautifulSoup(raw_html, "html.parser")
            
            for script in soup(["script", "style", "nav", "footer", "header"]):
                script.extract()
            text = soup.get_text(separator=" ")
            title = soup.title.string if soup.title else "No Title"
            if title: title = title.replace("\n", " ").strip()

            clean_text = re.sub(r'\s+', ' ', text).strip()
            clean_title = re.sub(r'\s+', ' ', str(title)).strip()
            
            doc_id = count
            
            f.write(f"{doc_id}\t{url}\t{clean_title}\t{clean_text}\n")
            
            count += 1
            if count % 1000 == 0:
                print(f"Экспортировано {count} документов...")
                
        except Exception as e:
            print(f"Ошибка в документе {doc.get('url')}: {e}")

print(f"Готово! Сохранено {count} документов в {output_file}")
