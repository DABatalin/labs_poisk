from pymongo import MongoClient
import re

def clean_database():
    client = MongoClient("localhost", 27017)
    collection = client.search_engine_db.raw_docs
    ban_phrase = "Flood detected"
    
    print("Ищем заблокированные документы...")
    
    query = {"raw_html": {"$regex": ban_phrase}, "source": "opennet"}
    banned_docs = list(collection.find(query))
    
    if not banned_docs:
        return

    print(f"Найдено {len(banned_docs)} 'битых' документов.")
    
    ids = []
    for doc in banned_docs:
        url = doc['url']
        try:
            num = int(url.split('num=')[-1])
            ids.append(num)
        except:
            pass
            
    if ids:
        min_id = min(ids)
        max_id = max(ids)
        print(f"Диапазон испорченных ID: от {min_id} до {max_id}")
        print(f"СОВЕТ: Открой crawler_state_opennet.json и поменяй current_id на {min_id}")
    
    result = collection.delete_many(query)
    print(f"Удалено документов: {result.deleted_count}")

if __name__ == "__main__":
    clean_database()
