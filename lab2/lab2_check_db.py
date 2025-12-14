from pymongo import MongoClient

client = MongoClient("localhost", 27017)
db = client.search_engine_db
collection = db.raw_docs

count = collection.count_documents({})
print(f"=== Всего документов в базе: {count} ===")
print("-" * 30)

habr_count = collection.count_documents({"source": "habr"})
opennet_count = collection.count_documents({"source": "opennet"})
print(f"Habr: {habr_count}")
print(f"OpenNet: {opennet_count}")
print("-" * 30)

last_doc = collection.find_one(sort=[("_id", -1)])

if last_doc:
    print("ПРИМЕР ПОСЛЕДНЕГО ДОКУМЕНТА:")
    print(f"URL: {last_doc.get('url')}")
    print(f"Source: {last_doc.get('source')}")
    print(f"Date crawled: {last_doc.get('crawled_at')}")
    
    html_preview = last_doc.get('raw_html', '')[500:800].replace('\n', ' ')
    print(f"HTML Preview: {html_preview}...")
else:
    print("База пуста.")
