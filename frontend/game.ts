/* ================================================================
   Comfy Factory – Conveyor Belt UI
   ================================================================ */

// ---------- Types ----------

interface GameState {
    phase: "playing" | "gameover" | "gameclear";
    orderNumber: number;
    score: number;
    mistakes: number;
    maxMistakes: number;
    maxOrders: number;
    currentStep: number;
    totalSteps: number;
    stepCategories: string[];
    order: Record<string, string>;
    current: Record<string, string>;
}

interface SubmitResult extends GameState {
    correct: boolean;
}

// ---------- Constants ----------

const CATEGORY_NAMES: Record<string, string> = {
    base: "ベース",
    cream: "クリーム",
    topping: "トッピング",
    decoration: "飾り",
    sauce: "ソース",
};

const INGREDIENT_NAMES: Record<string, Record<string, string>> = {
    base:       { vanilla: "バニラ", chocolate: "チョコ", strawberry: "ストロベリー" },
    cream:      { whipped: "ホイップ", chocolate: "チョコ", strawberry: "いちご" },
    topping:    { cherry: "チェリー", cookie: "クッキー", nuts: "ナッツ" },
    decoration: { sprinkles: "スプリンクル", chocolate_chips: "チョコチップ", star: "スター" },
    sauce:      { caramel: "キャラメル", chocolate: "チョコ", strawberry: "ストロベリー" },
};

const INGREDIENTS: Record<string, string[]> = {
    base:       ["vanilla", "chocolate", "strawberry"],
    cream:      ["whipped", "chocolate", "strawberry"],
    topping:    ["cherry", "cookie", "nuts"],
    decoration: ["sprinkles", "chocolate_chips", "star"],
    sauce:      ["caramel", "chocolate", "strawberry"],
};

// ---------- State ----------

let currentState: GameState | null = null;
let pendingSelection: string | null = null;
let isAnimating = false;

// ---------- API ----------

async function api<T>(method: string, path: string, body?: object): Promise<T> {
    const opts: RequestInit = { method, headers: { "Content-Type": "application/json" } };
    if (body) opts.body = JSON.stringify(body);
    const res = await fetch(path, opts);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json() as Promise<T>;
}

const getState  = () => api<GameState>("GET",  "/api/state");
const newGame   = () => api<GameState>("POST", "/api/new-game");
const selectApi = (cat: string, val: string) =>
    api<GameState>("POST", "/api/select", { category: cat, value: val });
const undoApi   = () => api<GameState>("POST", "/api/undo");
const submitApi = () => api<SubmitResult>("POST", "/api/submit");

// ---------- Cake rendering ----------

function cakeLayerHTML(category: string, value: string): string {
    return `<div class="cake-layer cake-${category} cake-${category}-${value}"></div>`;
}

function renderCakeVisual(ingredients: Record<string, string>, steps: string[]): string {
    let html = '<div class="cake">';
    for (const step of steps) {
        if (ingredients[step]) {
            html += cakeLayerHTML(step, ingredients[step]);
        }
    }
    html += "</div>";
    return html;
}

/** Merge confirmed state + pending selection for display */
function getDisplayedCake(): Record<string, string> {
    if (!currentState) return {};
    const cake = { ...currentState.current };
    if (pendingSelection != null && currentState.currentStep < currentState.totalSteps) {
        cake[currentState.stepCategories[currentState.currentStep]] = pendingSelection;
    }
    return cake;
}

// ---------- Renderers ----------

function renderOrderCard(): void {
    if (!currentState) return;
    document.getElementById("order-cake")!.innerHTML =
        renderCakeVisual(currentState.order, currentState.stepCategories);
}

function renderStepProgress(): void {
    if (!currentState) return;
    const el = document.getElementById("step-progress")!;
    let html = '<div class="progress-track">';

    for (let i = 0; i < currentState.totalSteps; i++) {
        const cat  = currentState.stepCategories[i];
        const name = CATEGORY_NAMES[cat] ?? cat;

        /* connector line */
        if (i > 0) {
            const lineActive = i <= currentState.currentStep ? " active" : "";
            html += `<div class="step-connector${lineActive}"></div>`;
        }

        /* dot */
        let cls = "step-dot";
        if (i < currentState.currentStep) cls += " completed";
        else if (i === currentState.currentStep) {
            cls += " current";
            if (pendingSelection != null) cls += " has-pending";
        }

        html += `<div class="step-item">
                    <div class="${cls}"></div>
                    <div class="step-label">${name}</div>
                 </div>`;
    }

    html += "</div>";
    el.innerHTML = html;
}

function renderAssembly(): void {
    if (!currentState) return;
    document.getElementById("assembly-cake")!.innerHTML =
        renderCakeVisual(getDisplayedCake(), currentState.stepCategories);
}

function renderIngredients(): void {
    if (!currentState) return;
    const btnsEl = document.getElementById("ingredients-buttons")!;

    if (currentState.phase !== "playing" ||
        currentState.currentStep >= currentState.totalSteps) {
        btnsEl.innerHTML = "";
        return;
    }

    const cat   = currentState.stepCategories[currentState.currentStep];
    const items = INGREDIENTS[cat] ?? [];
    let html = "";

    for (const item of items) {
        const label    = INGREDIENT_NAMES[cat]?.[item] ?? item;
        const selected = pendingSelection === item ? " selected" : "";
        html += `<button class="ingredient-btn ingredient-${cat}-${item}${selected}"
                         data-category="${cat}" data-value="${item}">
                    ${label}
                 </button>`;
    }
    btnsEl.innerHTML = html;
}

function renderNavButtons(): void {
    if (!currentState) return;
    const prevBtn = document.getElementById("prev-btn") as HTMLButtonElement;
    const nextBtn = document.getElementById("next-btn") as HTMLButtonElement;
    const playing = currentState.phase === "playing";
    const allDone = currentState.currentStep >= currentState.totalSteps;

    /* ← : disabled when step 0 AND no pending */
    prevBtn.disabled = !playing ||
        (currentState.currentStep <= 0 && pendingSelection == null);
    prevBtn.textContent = "\u25C0";  /* ◀ */

    /* → : changes to "提出" when all steps confirmed */
    if (allDone && playing) {
        nextBtn.disabled = false;
        nextBtn.textContent = "提出";
        nextBtn.classList.add("submit-mode");
    } else {
        nextBtn.classList.remove("submit-mode");
        nextBtn.textContent = "\u25B6";  /* ▶ */
        nextBtn.disabled = !playing || pendingSelection == null;
    }
}

function renderStatus(): void {
    if (!currentState) return;
    document.getElementById("order-info")!.textContent =
        `注文 ${currentState.orderNumber}/${currentState.maxOrders}`;
    document.getElementById("score-info")!.textContent =
        `スコア: ${currentState.score}`;

    const remaining = currentState.maxMistakes - currentState.mistakes;
    let hearts = "";
    for (let i = 0; i < currentState.maxMistakes; i++) {
        hearts += i < remaining ? "\u2665 " : "\u2661 ";
    }
    document.getElementById("mistakes-display")!.innerHTML =
        `ライフ: <span class="hearts">${hearts}</span>`;
}

function renderOverlay(): void {
    if (!currentState) return;
    const overlay = document.getElementById("overlay")!;
    const title   = document.getElementById("overlay-title")!;
    const message = document.getElementById("overlay-message")!;

    if (currentState.phase === "gameover") {
        overlay.classList.remove("hidden");
        title.textContent   = "ゲームオーバー";
        message.textContent = `スコア: ${currentState.score}`;
    } else if (currentState.phase === "gameclear") {
        overlay.classList.remove("hidden");
        title.textContent   = "ゲームクリア！";
        message.textContent = "おめでとう！全ての注文を完成させました！";
    } else {
        overlay.classList.add("hidden");
    }
}

/** Full render (after API call) */
function render(state: GameState): void {
    currentState = state;
    renderOrderCard();
    renderStepProgress();
    renderAssembly();
    renderIngredients();
    renderNavButtons();
    renderStatus();
    renderOverlay();
}

/** Light render (visual-only, no API) */
function updateVisuals(): void {
    renderStepProgress();
    renderAssembly();
    renderIngredients();
    renderNavButtons();
}

// ---------- Feedback ----------

function showFeedback(type: "correct" | "incorrect"): void {
    const el = document.getElementById("feedback")!;
    el.textContent = type === "correct" ? "正解！" : "不正解...";
    el.className = `feedback feedback-${type}`;
    setTimeout(() => el.classList.add("hidden"), 1200);
}

// ---------- Event handlers ----------

function handleIngredientClick(category: string, value: string): void {
    if (isAnimating || !currentState || currentState.phase !== "playing") return;
    pendingSelection = value;
    updateVisuals();
}

async function handlePrev(): Promise<void> {
    if (isAnimating || !currentState || currentState.phase !== "playing") return;

    /* If there's a pending (unconfirmed) selection, just clear it */
    if (pendingSelection != null) {
        pendingSelection = null;
        updateVisuals();
        return;
    }

    /* Otherwise undo the last confirmed step */
    if (currentState.currentStep <= 0) return;
    try {
        const state = await undoApi();
        pendingSelection = null;
        render(state);
    } catch (e) { console.error(e); }
}

async function handleNext(): Promise<void> {
    if (isAnimating || !currentState || currentState.phase !== "playing") return;

    /* All steps confirmed → submit */
    if (currentState.currentStep >= currentState.totalSteps) {
        isAnimating = true;
        try {
            const result = await submitApi();
            showFeedback(result.correct ? "correct" : "incorrect");
            setTimeout(() => {
                pendingSelection = null;
                render(result);
                isAnimating = false;
            }, 1300);
        } catch (e) {
            console.error(e);
            isAnimating = false;
        }
        return;
    }

    /* Confirm pending selection */
    if (pendingSelection == null) return;
    try {
        const cat = currentState.stepCategories[currentState.currentStep];
        const state = await selectApi(cat, pendingSelection);
        pendingSelection = null;
        render(state);
    } catch (e) { console.error(e); }
}

async function handleNewGame(): Promise<void> {
    try {
        pendingSelection = null;
        const state = await newGame();
        render(state);
    } catch (e) { console.error(e); }
}

// ---------- Bootstrap ----------

async function init(): Promise<void> {
    document.getElementById("prev-btn")!.addEventListener("click", handlePrev);
    document.getElementById("next-btn")!.addEventListener("click", handleNext);
    document.getElementById("new-game-btn")!.addEventListener("click", handleNewGame);

    document.getElementById("ingredients-buttons")!.addEventListener("click", (e) => {
        const btn = (e.target as HTMLElement).closest(".ingredient-btn") as HTMLElement | null;
        if (!btn) return;
        handleIngredientClick(btn.dataset.category!, btn.dataset.value!);
    });

    try {
        const state = await getState();
        render(state);
    } catch {
        document.getElementById("assembly-cake")!.textContent =
            "サーバーに接続できません";
    }
}

window.addEventListener("DOMContentLoaded", init);
