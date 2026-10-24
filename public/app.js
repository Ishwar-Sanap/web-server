const helloButton = document.querySelector("#hello-button");
const message = document.querySelector("#message");

helloButton.addEventListener("click", () => {
    message.textContent = "Hello from your C++ web server!";
});