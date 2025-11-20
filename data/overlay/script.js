// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
function mmss(ms) {
  ms = Math.max(0, ms | 0);
  const m = Math.floor(ms / 60000);
  const s = Math.floor((ms % 60000) / 1000);
  return `${String(m).padStart(2, "0")}:${String(s).padStart(2, "0")}`;
}

async function fetchState() {
  const res = await fetch("plugin.json", { cache: "no-store" });
  if (!res.ok) throw new Error("fetch failed");
  return await res.json();
}

function liveTimerMs(timer) {
  if (!timer) return 0;

  const mode = timer.mode || "countdown";
  const running = !!timer.running;
  const baseRemaining = Number(timer.remaining_ms ?? 0);
  const lastTick = Number(timer.last_tick_ms ?? 0);

  // If not running or no tick info, just trust remaining_ms
  if (!running || !lastTick) {
    return baseRemaining;
  }

  const now = Date.now();
  const delta = now - lastTick;

  if (mode === "countup") {
    return baseRemaining + delta;
  }

  const v = baseRemaining - delta;
  return v < 0 ? 0 : v;
}

// -----------------------------------------------------------------------------
// Expression engine
// -----------------------------------------------------------------------------
function resolvePath(obj, path) {
  if (!obj || !path) return undefined;

  const re = /([^[.\]]+)|\[(\d+)\]/g;
  let cur = obj;
  let m;

  while ((m = re.exec(path)) !== null) {
    if (m[1]) {
      cur = cur ? cur[m[1]] : undefined;
    } else if (m[2]) {
      const idx = Number(m[2]);
      if (!Array.isArray(cur)) return undefined;
      cur = cur[idx];
    }
    if (cur == null) return cur;
  }

  return cur;
}

function stripQuotes(s) {
  if (!s) return s;
  s = s.trim();
  if (
    (s.startsWith('"') && s.endsWith('"')) ||
    (s.startsWith("'") && s.endsWith("'"))
  ) {
    return s.slice(1, -1);
  }
  return s;
}

function isTruthyValue(val) {
  if (val === null || val === undefined) return false;
  if (typeof val === "string") return val.trim() !== "";
  return !!val;
}

/**
 * Evaluate an expression used inside {{ ... }} for text/attributes.
 * Supports:
 *   path                → team_x.logo, timers[0].mmss, fields_xy[1].x, etc.
 *   cond ? 'a' : 'b'    → swap_sides ? 'foo' : 'bar'
 *   !path               → returns boolean true/false, but will get stringified when used
 */
function evaluateExpression(expr, data) {
  if (!expr) return "";

  expr = expr.trim();

  // Ternary: cond ? 'a' : 'b'
  const ternaryMatch = expr.match(
    /^(.+?)\s*\?\s*(['"].*?['"])\s*:\s*(['"].*?['"])$/
  );
  if (ternaryMatch) {
    let condExpr = ternaryMatch[1].trim();
    const trueLit = stripQuotes(ternaryMatch[2]);
    const falseLit = stripQuotes(ternaryMatch[3]);

    let negate = false;
    if (condExpr.startsWith("!")) {
      negate = true;
      condExpr = condExpr.slice(1).trim();
    }

    const condVal = resolvePath(data, condExpr);
    const base = isTruthyValue(condVal);
    return (negate ? !base : base) ? trueLit : falseLit;
  }

  // Simple boolean: !path (returns true/false)
  if (expr.startsWith("!")) {
    const val = resolvePath(data, expr.slice(1).trim());
    return !isTruthyValue(val);
  }

  // Simple path
  const v = resolvePath(data, expr);
  return v == null ? "" : v;
}

/**
 * Evaluate a pure boolean condition for fs-if
 * Supports:
 *   fs-if="path"
 *   fs-if="!path"
 */
function evaluateCondition(expr, data) {
  if (!expr) return false;
  expr = expr.trim();

  if (expr.startsWith("!")) {
    const path = expr.slice(1).trim();
    const val = resolvePath(data, path);
    return !isTruthyValue(val);
  }

  const val = resolvePath(data, expr);
  return isTruthyValue(val);
}

// -----------------------------------------------------------------------------
// Template engine ({{ }}) and fs-if
// -----------------------------------------------------------------------------
const templateBindings = [];
const ifBindings = [];

function createBinding(targetNode, kind, attrName, templateString) {
  const parts = [];
  const regex = /{{\s*([^}]+?)\s*}}/g;
  let lastIndex = 0;
  let match;

  while ((match = regex.exec(templateString)) !== null) {
    if (match.index > lastIndex) {
      parts.push({
        type: "literal",
        value: templateString.slice(lastIndex, match.index),
      });
    }
    parts.push({
      type: "expr",
      value: match[1].trim(),
    });
    lastIndex = regex.lastIndex;
  }

  if (!parts.length) return null;

  if (lastIndex < templateString.length) {
    parts.push({
      type: "literal",
      value: templateString.slice(lastIndex),
    });
  }

  return { targetNode, kind, attrName, parts };
}

function collectTemplateBindings() {
  // Text nodes
  const walker = document.createTreeWalker(
    document.body,
    NodeFilter.SHOW_TEXT,
    null
  );

  let node;
  while ((node = walker.nextNode())) {
    const text = node.nodeValue;
    if (text && text.includes("{{")) {
      const binding = createBinding(node, "text", null, text);
      if (binding) templateBindings.push(binding);
    }
  }

  // Attributes (including fs-if)
  const all = document.querySelectorAll("*");
  all.forEach((el) => {
    for (const attr of el.attributes) {
      if (attr.name === "fs-if") {
        ifBindings.push({ el, expr: attr.value.trim() });
        continue;
      }

      const val = attr.value;
      if (val && val.includes("{{")) {
        const binding = createBinding(el, "attr", attr.name, val);
        if (binding) templateBindings.push(binding);
      }
    }
  });
}

function applyTemplateBindings(data) {
  if (!data) return;

  for (const b of templateBindings) {
    let str = "";

    for (const part of b.parts) {
      if (part.type === "literal") {
        str += part.value;
      } else {
        const v = evaluateExpression(part.value, data);
        str += v == null ? "" : String(v);
      }
    }

    if (b.kind === "text") {
      b.targetNode.nodeValue = str;
    } else if (b.kind === "attr") {
      b.targetNode.setAttribute(b.attrName, str);
    }
  }
}

function applyIfBindings(data) {
  if (!data) return;

  for (const b of ifBindings) {
    const ok = evaluateCondition(b.expr, data);

    if (!ok) {
      // Hide and mark aria-hidden, but keep in DOM so we can re-show later
      b.el.style.display = "none";
      b.el.setAttribute("aria-hidden", "true");
    } else {
      b.el.style.display = "";
      b.el.removeAttribute("aria-hidden");
    }
  }
}

// -----------------------------------------------------------------------------
// Rendering
// -----------------------------------------------------------------------------
let currentJsonState = null;

function renderFrame() {
  if (!currentJsonState) return;

  // Deep clone to avoid mutating original
  const st = JSON.parse(JSON.stringify(currentJsonState));

  // Ensure timers array
  if (!Array.isArray(st.timers)) {
    st.timers = [];
  }

  // Compute live timer values (mm:ss)
  for (let i = 0; i < st.timers.length; i++) {
    const t = st.timers[i] || {};
    const ms = liveTimerMs(t);
    t.live_ms = ms;
    t.mmss = mmss(ms);
    st.timers[i] = t;
  }

  // ---------------------------------------------------------------------------
  // Build derived view model:
  //   - team_x / team_y: left/right teams based on swap_sides
  //   - fields_xy: each custom field mapped to x/y based on swap_sides
  // ---------------------------------------------------------------------------
  const baseHome = st.home || {};
  const baseAway = st.away || {};
  const swap = !!st.swap_sides;

  const team_x = swap ? baseAway : baseHome; // left side
  const team_y = swap ? baseHome : baseAway; // right side

  const fields_xy = Array.isArray(st.custom_fields)
    ? st.custom_fields.map((cf) => {
      const leftVal = swap ? cf.away : cf.home;
      const rightVal = swap ? cf.home : cf.away;
      return {
        ...cf,
        x: leftVal,
        y: rightVal,
      };
    })
    : [];

  const view = {
    ...st,
    team_x,
    team_y,
    fields_xy,
  };

  // First apply fs-if (visibility), then template bindings (text/attrs)
  applyIfBindings(view);
  applyTemplateBindings(view);

  const board = document.getElementById("scoreboard");
  if (board) {
    if (view.show_scoreboard) {
      board.classList.remove("is-hidden");
      board.setAttribute("aria-hidden", "false");
    } else {
      board.classList.add("is-hidden");
      board.setAttribute("aria-hidden", "true");
    }
  }
}

// -----------------------------------------------------------------------------
// Poll loop
// -----------------------------------------------------------------------------
async function pollLoop() {
  try {
    const st = await fetchState();
    currentJsonState = st;
  } catch (e) {
    // ignore; keep last state
  } finally {
    setTimeout(pollLoop, 1000);
  }
}

function animationLoop() {
  renderFrame();
  requestAnimationFrame(animationLoop);
}

// -----------------------------------------------------------------------------
// Boot
// -----------------------------------------------------------------------------
collectTemplateBindings();
pollLoop();
animationLoop();
