// Fly Scoreboard docs – smooth scroll + active TOC highlight

// Smooth scroll for TOC links
const tocLinks = document.querySelectorAll(".toc-link[href^='#']");

tocLinks.forEach((link) => {
  link.addEventListener("click", (e) => {
    e.preventDefault();
    const id = link.getAttribute("href").slice(1);
    const target = document.getElementById(id);
    if (!target) return;

    target.scrollIntoView({
      behavior: "smooth",
      block: "start",
    });
  });
});

// Active state sync using IntersectionObserver
const sections = document.querySelectorAll("section[data-section]");
const sectionById = {};
sections.forEach((s) => {
  sectionById[s.id] = s;
});

const observer = new IntersectionObserver(
  (entries) => {
    entries.forEach((entry) => {
      if (!entry.isIntersecting) return;
      const id = entry.target.id;

      tocLinks.forEach((link) => {
        const hrefId = link.getAttribute("href").slice(1);
        link.classList.toggle("active", hrefId === id);
      });
    });
  },
  {
    rootMargin: "-50% 0px -40% 0px",
    threshold: 0.01,
  }
);

sections.forEach((section) => observer.observe(section));
