const links = document.querySelectorAll('nav.links a[href^="#"]');
links.forEach((a) => {
  a.addEventListener("click", (e) => {
    e.preventDefault();
    const id = a.getAttribute("href").slice(1);
    const el = document.getElementById(id);
    if (el) el.scrollIntoView({ behavior: "smooth", block: "start" });
  });
});

const obs = new IntersectionObserver(
  (entries) => {
    entries.forEach((e) => {
      if (e.isIntersecting) {
        const id = e.target.id;
        document
          .querySelectorAll("nav.links a")
          .forEach((a) =>
            a.classList.toggle("active", a.getAttribute("href") === "#" + id)
          );
      }
    });
  },
  { rootMargin: "-60% 0px -35% 0px", threshold: 0.01 }
);

document.querySelectorAll("section[id]").forEach((s) => obs.observe(s));
