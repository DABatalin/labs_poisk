import yaml
import time
import requests
import json
import os
import random
import sys
from datetime import datetime
from pymongo import MongoClient

class Crawler:
    def __init__(self, config_path, target_source):
        self.target = target_source 
        self.load_config(config_path)
        self.setup_db()
        self.load_state()
        self.my_user_agent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 YaBrowser/25.10.0.0 Safari/537.36"
        
    def load_config(self, path):
        with open(path, 'r', encoding='utf-8') as f:
            self.cfg = yaml.safe_load(f)
            
    def setup_db(self):
        db_cfg = self.cfg['db']
        client = MongoClient(db_cfg['host'], db_cfg['port'])
        self.db = client[db_cfg['name']]
        self.collection = self.db[db_cfg['collection']]
        self.collection.create_index("url", unique=True)
        print(f"[{self.target.upper()}] Connected to MongoDB.")

    def load_state(self):
        base_name = self.cfg['logic']['state_file']
        name_part, ext = os.path.splitext(base_name)
        self.state_file_path = f"{name_part}_{self.target}{ext}"
        self.current_id = self.cfg['sources'][self.target]['start_id']
        
        if os.path.exists(self.state_file_path):
            with open(self.state_file_path, 'r') as f:
                data = json.load(f)
                if 'current_id' in data:
                    self.current_id = data['current_id']
        print(f"[{self.target.upper()}] Resuming from ID: {self.current_id}")

    def save_state(self):
        data = {'current_id': self.current_id}
        with open(self.state_file_path, 'w') as f:
            json.dump(data, f)

    def get_headers(self):
        return {
            'User-Agent': self.my_user_agent,
            'Referer': 'https://www.google.com/',
            'Accept-Language': 'ru,en;q=0.9',
            'Connection': 'keep-alive'
        }

    def random_sleep(self, multiplier=1.0):
        source_delay = self.cfg['sources'][self.target].get('delay', 2.0)
        base_delay = source_delay * multiplier
        jitter = base_delay * 0.3
        actual_delay = base_delay + random.uniform(-jitter, jitter)
        
        if actual_delay < 0.1: actual_delay = 0.1
        time.sleep(actual_delay)

    def is_banned(self, html):
        if not html: return False
        
        if self.target == 'opennet':
            ban_markers = [
                "Flood detected",
                "detected flood from you",
                "высокой паразитной нагрузке",
                "Stop it."
            ]
            for marker in ban_markers:
                if marker in html:
                    return True
                    
        elif self.target == 'habr':
            if "Qrator.AntiBot" in html or "DDOS-GUARD" in html:
                return True
            return False

        return False

    def download_url(self, url):
        try:
            resp = requests.get(url, headers=self.get_headers(), timeout=15)
            
            if resp.status_code == 404:
                return "404"
            
            if resp.status_code == 403:
                if self.target == 'habr':
                    return "404" 
                else:
                    print(f"\n[!!!] HTTP BAN STATUS: {resp.status_code}")
                    return "BAN"

            if resp.status_code in [429, 503]:
                print(f"\n[!!!] HTTP BAN STATUS: {resp.status_code}")
                return "BAN"

            if self.target == 'opennet':
                resp.encoding = resp.apparent_encoding
            else:
                resp.encoding = 'utf-8'

            resp.raise_for_status()
            return resp.text
            
        except Exception as e:
            print(f"\n[!] Error fetching {url}: {e}")
            return None

    def save_document(self, url, html):
        doc = {
            "url": url,
            "raw_html": html,
            "source": self.target,
            "crawled_at": int(time.time())
        }
        try:
            self.collection.update_one({"url": url}, {"$set": doc}, upsert=True)
            print("Saved.")
        except Exception as e:
            print(f"DB Error: {e}")

    def run(self):
        print(f"--- Started Worker for {self.target.upper()} ---")
        end_id = self.cfg['sources'][self.target]['end_id']
        base_url_template = self.cfg['sources'][self.target]['base_url']
        step = self.cfg['sources'][self.target].get('step', 1)
        
        if step == 2 and self.current_id % 2 != 0:
            print(f"[{self.target}] Alignment: ID {self.current_id} is odd -> moving to {self.current_id + 1}")
            self.current_id += 1

        consecutive_bans = 0 

        try:
            while self.current_id < end_id:
                url = base_url_template.format(self.current_id)
                current_time = datetime.now().strftime("%H:%M:%S")
                
                print(f"[{current_time}] [{self.target}] ID {self.current_id}", end="... ")
                
                html = self.download_url(url)
                
                if html == "BAN" or self.is_banned(html):
                    print("\n[!!!] ALARM: BANNED by SYSTEM!")
                    print("[!!!] Sleeping for 600 seconds (10 min)...")
                    
                    time.sleep(600) 
                    
                    consecutive_bans += 1
                    if consecutive_bans > 3:
                        print("Too many bans. Exiting script.")
                        break
                    
                    continue
                
                consecutive_bans = 0 

                if html == "404":
                    print("Not Found/Hidden.")
                elif html:
                    self.save_document(url, html)
                else:
                    print("Skip (Network Error).")
                
                self.current_id += step
                self.save_state()
                self.random_sleep()
                
            print(f"[{self.target}] Finished all IDs!")
            
        except KeyboardInterrupt:
            print(f"\n[{self.target}] Stopping...")
            self.save_state()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python crawler.py [habr|opennet]")
        sys.exit(1)
    target = sys.argv[1].lower()
    crawler = Crawler("config.yaml", target)
    crawler.run()
