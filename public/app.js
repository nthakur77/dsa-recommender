document.addEventListener("DOMContentLoaded", () => {
    loadDashboard();

    document.getElementById("add-problem-form").addEventListener("submit", (e) => {
        e.preventDefault();
        
        const title = document.getElementById("newTitle").value;
        const topic = document.getElementById("newTopic").value;
        const diff = document.getElementById("newDiff").value;
        const url = document.getElementById("newUrl").value;

        const bodyData = `title=${encodeURIComponent(title)}&topic=${encodeURIComponent(topic)}&difficulty=${encodeURIComponent(diff)}&url=${encodeURIComponent(url)}`;

        fetch('/api/add_problem', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: bodyData
        })
        .then(response => response.text())
        .then(msg => {
            alert(msg); 
            document.getElementById("add-problem-form").reset(); 
            loadDashboard(); 
        })
        .catch(error => alert("Failed to connect to server: " + error));
    });
});

function loadDashboard() {
    fetch('/api/recommendations', { cache: 'no-store' })
        .then(response => {
            if (!response.ok) throw new Error("Server Error " + response.status);
            return response.json();
        })
        .then(problems => {
            const container = document.getElementById('recommendations-container');
            container.innerHTML = ''; 
            
            if (problems.length === 0) {
                container.innerHTML = '<p style="color: #22c55e;">You have solved all available problems for your current rating! Time to add more.</p>';
                return;
            }

            problems.forEach(p => {
                const card = document.createElement('div');
                card.className = 'problem-card';
                card.innerHTML = `
                    <h3 style="color: #ffa116; margin-bottom: 8px;">${p.title}</h3>
                    <p>Topic: <strong>${p.topic}</strong> | Rating: ${p.difficulty}</p>
                    <a href="${p.url}" target="_blank" style="margin: 10px 0; display: inline-block;">Solve on LeetCode</a>
                    <br>
                    <button onclick="markSolved(${p.id}, ${p.difficulty}, '${p.topic}')" style="width: 100%;">✔️ Log Solution</button>
                `;
                container.appendChild(card);
            });
        })
        .catch(error => {
            document.getElementById('recommendations-container').innerHTML = 
                `<p style="color: #ef4444; padding: 10px; border: 1px solid #ef4444; border-radius: 5px;">Failed to load data: ${error.message}</p>`;
        });

    fetch('/api/history', { cache: 'no-store' })
        .then(response => {
            if (!response.ok) throw new Error("Server Error " + response.status);
            return response.json();
        })
        .then(data => {
            document.getElementById('total-solved-count').innerText = data.total_solved;
            
            const historyList = document.getElementById('history-container');
            historyList.innerHTML = '';
            data.history.forEach(h => {
                historyList.innerHTML += `
                    <li>
                        <span style="color: #e2e8f0;">${h.title}</span>
                        <span style="color: #94a3b8; font-size: 0.9em;">${h.time}m</span>
                    </li>
                `;
            });
        })
        .catch(error => console.error("History fetch failed:", error));
}

function markSolved(id, difficulty, topic) {
    const timeTaken = prompt("Time taken (minutes)?", "20");
    if (!timeTaken) return;

    const bodyData = `problemId=${id}&timeTaken=${timeTaken}&difficulty=${difficulty}&topic=${encodeURIComponent(topic)}`;

    fetch('/api/solve', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: bodyData
    })
    .then(response => response.text())
    .then(msg => {
        loadDashboard(); 
    })
    .catch(error => alert("Failed to log solution: " + error));
}