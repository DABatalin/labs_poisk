from flask import Flask, request, render_template_string
import subprocess
import os

app = Flask(__name__)

SEARCHER_BIN = "./searcher" 

HTML_TEMPLATE = """
<!doctype html>
<html lang="ru">
<head>
    <meta charset="utf-8">
    <title>My Boolean Search</title>
    <style>
        body { font-family: sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; }
        .search-box { margin-bottom: 20px; }
        input[type="text"] { width: 70%; padding: 10px; font-size: 16px; }
        input[type="submit"] { padding: 10px 20px; font-size: 16px; }
        .result { margin-bottom: 20px; border-bottom: 1px solid #eee; padding-bottom: 10px; }
        .result a { font-size: 18px; color: #1a0dab; text-decoration: none; }
        .result a:hover { text-decoration: underline; }
        .url { color: #006621; font-size: 14px; }
        .meta { color: #777; font-size: 12px; margin-top: 10px;}
        .pagination { margin-top: 20px; }
        .error { color: red; }
    </style>
</head>
<body>
    <h1>Boolean Search Engine</h1>
    
    <div class="search-box">
        <form action="/" method="get">
            <input type="text" name="q" value="{{ query }}" placeholder="Введите запрос (например: java && !script)...">
            <input type="submit" value="Найти">
        </form>
    </div>

    {% if query %}
        <div class="meta">Найдено документов: {{ total_count }} (за {{ time_ms }} мс)</div>
        <hr>
        
        {% for res in results %}
            <div class="result">
                <div><a href="{{ res.url }}">{{ res.title }}</a></div>
                <div class="url">{{ res.url }}</div>
            </div>
        {% endfor %}

        {% if results|length == 0 %}
            <p>Ничего не найдено.</p>
        {% endif %}

        <div class="pagination">
            {% if prev_offset >= 0 %}
                <a href="/?q={{ query }}&offset={{ prev_offset }}">← Назад</a>
            {% endif %}
            
            {% if next_offset < total_count %}
                <span style="margin: 0 10px;"></span>
                <a href="/?q={{ query }}&offset={{ next_offset }}">Вперед →</a>
            {% endif %}
        </div>
    {% endif %}
</body>
</html>
"""

@app.route('/')
def search():
    query = request.args.get('q', '')
    offset = int(request.args.get('offset', 0))
    limit = 50
    
    results = []
    total_count = 0
    time_ms = 0
    
    if query:
        try:
            cmd = [SEARCHER_BIN, "--web", query, str(offset), str(limit)]
            process = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8')
            
            if process.returncode != 0:
                return f"Error executing searcher: {process.stderr}"
            
            lines = process.stdout.strip().splitlines()
            if len(lines) >= 2:
                total_count = int(lines[0])
                time_ms = lines[1]
                
                for line in lines[2:]:
                    parts = line.split('\t')
                    if len(parts) >= 2:
                        results.append({'url': parts[0], 'title': parts[1]})
                    elif len(parts) == 1:
                        results.append({'url': parts[0], 'title': "No Title"})
                        
        except Exception as e:
            return f"System Error: {e}"

    return render_template_string(HTML_TEMPLATE, 
                                  query=query, 
                                  results=results, 
                                  total_count=total_count,
                                  time_ms=time_ms,
                                  next_offset = offset + limit,
                                  prev_offset = offset - limit)

if __name__ == '__main__':
    app.run(debug=True, port=5000)
