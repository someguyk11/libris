document.addEventListener("DOMContentLoaded", () => {
    const toggleBtn = document.getElementById("filterToggleBtn");
    const drawer = document.getElementById("filterDrawer");

    const burgerBtn = document.getElementById("mobileBurgerBtn");
    const sidebarNode = document.getElementById("sidebarNode");
    const backdropNode = document.getElementById("menuBackdrop");

    if (burgerBtn && sidebarNode && backdropNode) {
        function toggleMobileMenu() {
            burgerBtn.classList.toggle("is-active");
            sidebarNode.classList.toggle("mobile-open");
            backdropNode.classList.toggle("active");
        }
        burgerBtn.addEventListener("click", toggleMobileMenu);
        backdropNode.addEventListener("click", toggleMobileMenu);
    }
  }
)