(function () {
  const routes = {
    "overview": { title: "Overview", file: "pages/overview.html", subtitle: "What Fly Scoreboard is and how the overlay workflow works." },
    "download-install": { title: "Download & Install", file: "pages/download-install.html", subtitle: "Get the plugin + install it into OBS Studio." },
    "setup": { title: "How to Setup", file: "pages/setup.html", subtitle: "Create a Browser Source and connect it to the Fly Scoreboard dock." },
    "usage": { title: "How to Use", file: "pages/usage.html", subtitle: "Operate teams, scores, timers, and stats during a live show." },
    "add-scores": { title: "Add New Scores", file: "pages/add-scores.html", subtitle: "Use Custom Fields to add score rows like fouls, shots, and penalties." },
    "add-timers": { title: "Add New Timers", file: "pages/add-timers.html", subtitle: "Configure multiple timers and render them safely in your overlay." },
    "templating": { title: "Templating System", file: "pages/templating.html", subtitle: "How {{…}} placeholders and fs-if conditions are evaluated." },
    "ai-prompt": { title: "AI Prompt", file: "pages/ai-prompt.html", subtitle: "Copy/paste prompt to generate Fly Scoreboard compatible overlays." },
  };

  const pillMap = {
    "overview": ["OBS", "Browser Source", "plugin.json", "Templates"],
    "download-install": ["Download", "Install", "OBS", "Overlay"],
    "setup": ["Setup", "Browser Source", "Paths", "Troubleshooting"],
    "usage": ["Live workflow", "Teams", "Scores", "Timers"],
    "add-scores": ["Custom fields", "Home/Away", "fields_xy", "Visibility"],
    "add-timers": ["Timers", "countup/down", "mm:ss", "Smooth display"],
    "templating": ["{{…}}", "fs-if", "team_x/y", "Swap sides"],
    "ai-prompt": ["Prompt", "Overlay authoring", "Compatibility", "No loops"],
  };

  function getRoute() {
    const hash = (location.hash || "#overview").replace("#", "");
    return routes[hash] ? hash : "overview";
  }

  function setActiveNav(routeKey) {
    document.querySelectorAll(".nav__link[data-nav]").forEach((a) => {
      a.classList.toggle("is-active", a.getAttribute("href") === "#" + routeKey);
    });
  }

  function renderPills(routeKey) {
    const pills = document.querySelector("#docPills");
    if (!pills) return;
    pills.innerHTML = "";

    (pillMap[routeKey] || []).forEach((t) => {
      const span = document.createElement("span");
      span.className = "pill";
      span.textContent = t;
      pills.appendChild(span);
    });
  }

  async function load(routeKey) {
    const route = routes[routeKey];

    const mainTitle = document.querySelector("[data-page-title]");
    const mainSubtitle = document.querySelector("[data-page-subtitle]");
    const content = document.querySelector("#docContent");

    setActiveNav(routeKey);

    if (mainTitle) mainTitle.textContent = route.title;
    if (mainSubtitle) mainSubtitle.textContent = route.subtitle || "";

    renderPills(routeKey);

    try {
      const res = await fetch(route.file, { cache: "no-store" });
      if (!res.ok) throw new Error("HTTP " + res.status);

      const html = await res.text();
      if (content) content.innerHTML = html;

      document.title = route.title + " • Fly Scoreboard";
      window.scrollTo({ top: 0, behavior: "instant" });

      addCopyButtons();
    } catch (err) {
      if (content) {
        content.innerHTML =
          `<div class="warn"><strong>Could not load this page.</strong>` +
          `<div style="margin-top:6px;">Check that <code>${route.file}</code> exists and is being served by your host.</div></div>`;
      }
      console.error(err);
    }
  }

  // Mobile toggle
  const sidebar = document.getElementById("sidebar");
  const toggle = document.getElementById("sidebarToggle");
  if (toggle && sidebar) {
    toggle.addEventListener("click", () => {
      const open = sidebar.classList.toggle("is-open");
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
  }

  // Navigation search filter (keeps the "functionality" pattern from Fly docs)
  const navSearch = document.getElementById("navSearch");
  if (navSearch) {
    navSearch.addEventListener("input", () => {
      const q = navSearch.value.trim().toLowerCase();
      const links = Array.from(document.querySelectorAll(".nav__link"));
      links.forEach(a => {
        const text = (a.textContent || "").toLowerCase();
        a.style.display = !q || text.includes(q) ? "" : "none";
      });

      const sections = Array.from(document.querySelectorAll(".nav__section"));
      sections.forEach(sec => {
        let el = sec.nextElementSibling;
        let any = false;
        while (el && !el.classList.contains("nav__section")) {
          if (el.classList.contains("nav__link") && el.style.display !== "none") any = true;
          el = el.nextElementSibling;
        }
        sec.style.display = any || !q ? "" : "none";
      });
    });
  }

  // Copy buttons for code blocks
  function addCopyButtons(){
    const pres = document.querySelectorAll("pre");
    pres.forEach(pre => {
      if (pre.querySelector(".copy-btn")) return;

      const btn = document.createElement("button");
      btn.className = "copy-btn";
      btn.type = "button";
      btn.textContent = "Copy";
      btn.addEventListener("click", async () => {
        const code = pre.querySelector("code");
        const text = code ? code.innerText : pre.innerText;
        try{
          await navigator.clipboard.writeText(text);
          btn.textContent = "Copied";
          setTimeout(()=>btn.textContent="Copy", 1200);
        }catch(e){
          btn.textContent = "Failed";
          setTimeout(()=>btn.textContent="Copy", 1200);
        }
      });

      pre.appendChild(btn);
    });
  }
  window.addCopyButtons = addCopyButtons;

  window.addEventListener("hashchange", () => load(getRoute()));
  document.addEventListener("DOMContentLoaded", () => load(getRoute()));
})();