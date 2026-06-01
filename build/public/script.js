// ========================
// script.js — Task Planner
// ========================

let token = localStorage.getItem('token');
let currentSearch = '';
let currentSort = 'created_at_desc';

// DOM элементы
const authContainer = document.getElementById('authContainer');
const appContainer = document.getElementById('appContainer');
const loginFormDiv = document.getElementById('loginForm');
const registerFormDiv = document.getElementById('registerForm');
const loginBtn = document.getElementById('loginBtn');
const registerBtn = document.getElementById('registerBtn');
const showRegisterLink = document.getElementById('showRegisterLink');
const showLoginLink = document.getElementById('showLoginLink');
const logoutBtn = document.getElementById('logoutBtn');
const addTaskBtn = document.getElementById('addTaskBtn');
const applyFiltersBtn = document.getElementById('applyFiltersBtn');
const searchInput = document.getElementById('searchInput');
const sortSelect = document.getElementById('sortSelect');

function escapeHtml(str) {
    if (!str) return '';
    return str.replace(/[&<>]/g, function(m) {
        if (m === '&') return '&amp;';
        if (m === '<') return '&lt;';
        if (m === '>') return '&gt;';
        return m;
    });
}

function showLogin() {
    loginFormDiv.style.display = 'block';
    registerFormDiv.style.display = 'none';
}

function showRegister() {
    loginFormDiv.style.display = 'none';
    registerFormDiv.style.display = 'block';
}

function showAuth() {
    authContainer.style.display = 'flex';
    appContainer.style.display = 'none';
    showLogin();
}

function showApp() {
    authContainer.style.display = 'none';
    appContainer.style.display = 'block';
    loadTasks();
}

async function login() {
    const email = document.getElementById('loginEmail').value.trim();
    const password = document.getElementById('loginPassword').value;
    if (!email || !password) {
        alert('Введите email и пароль');
        return;
    }
    try {
        const res = await fetch('/auth/login', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ email, password })
        });
        const data = await res.json();
        if (res.ok) {
            token = data.access_token;
            localStorage.setItem('token', token);
            showApp();
        } else {
            alert(data.error || 'Ошибка входа');
        }
    } catch (err) {
        alert('Ошибка сети: ' + err.message);
    }
}

async function register() {
    const name = document.getElementById('regName').value.trim();
    const email = document.getElementById('regEmail').value.trim();
    const password = document.getElementById('regPassword').value;
    if (!email || !password) {
        alert('Заполните email и пароль');
        return;
    }
    if (password.length < 4) {
        alert('Пароль должен быть не менее 4 символов');
        return;
    }
    try {
        const res = await fetch('/auth/register', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ email, password, name: name || undefined })
        });
        if (res.ok) {
            alert('Регистрация успешна! Теперь войдите.');
            showLogin();
            document.getElementById('regEmail').value = '';
            document.getElementById('regPassword').value = '';
            document.getElementById('regName').value = '';
        } else {
            const err = await res.json();
            alert(err.error || 'Ошибка регистрации');
        }
    } catch (err) {
        alert('Ошибка сети: ' + err.message);
    }
}

function logout() {
    localStorage.removeItem('token');
    token = null;
    currentSearch = '';
    currentSort = 'created_at_desc';
    if (searchInput) searchInput.value = '';
    if (sortSelect) sortSelect.value = 'created_at_desc';
    showAuth();
    const tasksList = document.getElementById('tasksList');
    if (tasksList) tasksList.innerHTML = '';
}

async function loadTasks() {
    if (!token) return;
    try {
        const url = `/api/tasks?search=${encodeURIComponent(currentSearch)}&sort=${currentSort}`;
        const res = await fetch(url, {
            headers: { 'Authorization': `Bearer ${token}` }
        });
        if (res.status === 401) {
            logout();
            return;
        }
        const tasks = await res.json();
        const container = document.getElementById('tasksList');
        container.innerHTML = '';
        for (const task of tasks) {
            const li = document.createElement('li');
            // Класс 'completed' оставляем для стилизации (серый фон), но зачёркивания не будет
            li.className = `task priority-${task.priority} ${task.status === 'completed' ? 'completed' : ''}`;
            
            // Кнопка редактирования только для активных задач
            let editButtonHtml = '';
            if (task.status !== 'completed') {
                editButtonHtml = `<button class="task-btn edit-btn" data-id="${task.id}">✏️ Редактировать</button>`;
            }

            li.innerHTML = `
                <div class="task-title">
                    <strong>${escapeHtml(task.title)}</strong>
                    <span class="task-meta">(${task.priority === 'high' ? 'Высокий' : task.priority === 'medium' ? 'Средний' : 'Низкий'}) - ${task.status === 'completed' ? 'Выполнена' : 'Активна'}</span>
                </div>
                ${task.deadline ? `<div class="task-deadline">⏰ Дедлайн: ${new Date(task.deadline).toLocaleString()}</div>` : ''}
                <div class="task-actions">
                    <button class="task-btn complete-btn" data-id="${task.id}" data-status="${task.status}">${task.status === 'completed' ? '↩️ Вернуть' : '✅ Выполнить'}</button>
                    ${editButtonHtml}
                    <button class="task-btn delete-btn" data-id="${task.id}">🗑️ Удалить</button>
                </div>
                <div class="subtasks">
                    <div class="subtask-add">
                        <input type="text" class="subtask-input" id="subTitle-${task.id}" placeholder="Новая подзадача">
                        <button class="subtask-add-btn" data-id="${task.id}">➕ Добавить подзадачу</button>
                    </div>
                    <ul class="subtask-list" id="subtasks-${task.id}">
                        ${task.subtasks.map(st => `
                            <li>
                                ${escapeHtml(st.title)} - ${st.status === 'pending' ? '◻️' : '✅'}
                                <button class="subtask-toggle-btn" data-id="${st.id}">✓</button>
                            </li>
                        `).join('')}
                    </ul>
                </div>
            `;
            container.appendChild(li);
        }
        attachTaskEventHandlers();
    } catch (err) {
        console.error('loadTasks error', err);
    }
}

function attachTaskEventHandlers() {
    document.querySelectorAll('.complete-btn').forEach(btn => {
        btn.removeEventListener('click', handleComplete);
        btn.addEventListener('click', handleComplete);
    });
    document.querySelectorAll('.edit-btn').forEach(btn => {
        btn.removeEventListener('click', handleEdit);
        btn.addEventListener('click', handleEdit);
    });
    document.querySelectorAll('.delete-btn').forEach(btn => {
        btn.removeEventListener('click', handleDelete);
        btn.addEventListener('click', handleDelete);
    });
    document.querySelectorAll('.subtask-add-btn').forEach(btn => {
        btn.removeEventListener('click', handleAddSubtask);
        btn.addEventListener('click', handleAddSubtask);
    });
    document.querySelectorAll('.subtask-toggle-btn').forEach(btn => {
        btn.removeEventListener('click', handleToggleSubtask);
        btn.addEventListener('click', handleToggleSubtask);
    });
}

async function handleComplete(e) {
    const btn = e.currentTarget;
    const id = parseInt(btn.dataset.id);
    const currentStatus = btn.dataset.status;
    const newStatus = (currentStatus === 'completed') ? 'pending' : 'completed';
    await fetch(`/api/tasks/${id}/status`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${token}` },
        body: JSON.stringify({ status: newStatus })
    });
    loadTasks();
}

async function handleEdit(e) {
    const id = parseInt(e.currentTarget.dataset.id);
    try {
        const res = await fetch(`/api/tasks/${id}`, {
            headers: { 'Authorization': `Bearer ${token}` }
        });
        if (!res.ok) throw new Error('Не удалось получить задачу');
        const task = await res.json();
        const newTitle = prompt('Новое название', task.title);
        if (!newTitle) return;
        let newPriority = prompt('Приоритет (high/medium/low)', task.priority);
        if (!['high', 'medium', 'low'].includes(newPriority)) {
            alert('Приоритет должен быть high, medium или low');
            return;
        }
        let newDeadline = prompt('Дедлайн (формат YYYY-MM-DD HH:MM:00, оставьте пустым если нет)', task.deadline || '');
        if (newDeadline === '') newDeadline = null;
        const updateRes = await fetch(`/api/tasks/${id}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${token}` },
            body: JSON.stringify({ title: newTitle, priority: newPriority, deadline: newDeadline })
        });
        if (updateRes.ok) loadTasks();
        else alert('Ошибка редактирования');
    } catch (err) {
        alert('Ошибка: ' + err.message);
    }
}

async function handleDelete(e) {
    const id = parseInt(e.currentTarget.dataset.id);
    if (!confirm('Удалить задачу?')) return;
    await fetch(`/api/tasks/${id}`, { method: 'DELETE', headers: { 'Authorization': `Bearer ${token}` } });
    loadTasks();
}

async function handleAddSubtask(e) {
    const taskId = parseInt(e.currentTarget.dataset.id);
    const input = document.getElementById(`subTitle-${taskId}`);
    const title = input.value.trim();
    if (!title) return;
    await fetch(`/api/tasks/${taskId}/subtasks`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${token}` },
        body: JSON.stringify({ title, order: 0 })
    });
    input.value = '';
    loadTasks();
}

async function handleToggleSubtask(e) {
    const subId = parseInt(e.currentTarget.dataset.id);
    await fetch(`/api/subtasks/${subId}/status`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${token}` },
        body: JSON.stringify({ status: 'completed' })
    });
    loadTasks();
}

async function addTask() {
    const title = document.getElementById('title').value.trim();
    const priority = document.getElementById('priority').value;
    let deadline = document.getElementById('deadline').value;
    if (!title) {
        alert('Введите название задачи');
        return;
    }
    let formattedDeadline = null;
    if (deadline) {
        formattedDeadline = deadline.replace('T', ' ') + ':00';
    }
    try {
        const res = await fetch('/api/tasks', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${token}` },
            body: JSON.stringify({ title, priority, deadline: formattedDeadline })
        });
        if (res.ok) {
            loadTasks();
            document.getElementById('title').value = '';
            document.getElementById('deadline').value = '';
        } else {
            const err = await res.json();
            alert('Ошибка: ' + (err.error || 'Не удалось создать задачу'));
        }
    } catch (err) {
        alert('Ошибка сети: ' + err.message);
    }
}

function applyFilters() {
    currentSearch = searchInput ? searchInput.value : '';
    currentSort = sortSelect ? sortSelect.value : 'created_at_desc';
    loadTasks();
}

function init() {
    if (loginBtn) loginBtn.addEventListener('click', login);
    if (registerBtn) registerBtn.addEventListener('click', register);
    if (showRegisterLink) showRegisterLink.addEventListener('click', (e) => { e.preventDefault(); showRegister(); });
    if (showLoginLink) showLoginLink.addEventListener('click', (e) => { e.preventDefault(); showLogin(); });
    if (logoutBtn) logoutBtn.addEventListener('click', logout);
    if (addTaskBtn) addTaskBtn.addEventListener('click', addTask);
    if (applyFiltersBtn) applyFiltersBtn.addEventListener('click', applyFilters);
    if (searchInput) searchInput.addEventListener('keypress', (e) => { if (e.key === 'Enter') applyFilters(); });

    if (token) {
        showApp();
    } else {
        showAuth();
    }
}

document.addEventListener('DOMContentLoaded', init);