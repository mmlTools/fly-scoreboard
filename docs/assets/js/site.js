(function(){
  const sidebar = document.getElementById("sidebar");
  const toggle = document.getElementById("sidebarToggle");
  const navSearch = document.getElementById("navSearch");

  // Mobile toggle
  if (toggle && sidebar) {
    toggle.addEventListener("click", () => {
      const open = sidebar.classList.toggle("is-open");
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
  }

  // Active link detection
  try{
    const path = location.pathname.replace(/\\/g, "/");
    const file = path.split("/").pop() || "index.html";

    const map = {
      "index.html": "index",
      "download-install.html": "download-install",
      "setup.html": "setup",
      "usage.html": "usage",
      "add-scores.html": "add-scores",
      "add-timers.html": "add-timers",
      "templating.html": "templating",
      "ai-prompt.html": "ai-prompt",
    };

    const key = map[file];
    if (key) {
      const link = document.querySelector(`.nav__link[data-nav="${key}"]`);
      if (link) link.classList.add("is-active");
    }
  }catch(e){ /* ignore */ }

  // Navigation search filter
  if (navSearch) {
    navSearch.addEventListener("input", () => {
      const q = navSearch.value.trim().toLowerCase();
      const links = Array.from(document.querySelectorAll(".nav__link"));
      links.forEach(a => {
        const text = (a.textContent || "").toLowerCase();
        a.style.display = !q || text.includes(q) ? "" : "none";
      });

      // Hide section headers when everything under them is hidden
      const sections = Array.from(document.querySelectorAll(".nav__section"));
      sections.forEach(sec => {
        // Show if any following links until next section are visible
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
  addCopyButtons();
})();