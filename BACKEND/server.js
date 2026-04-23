const express = require("express");
const { exec } = require("child_process");
const path = require("path");

const app = express();

app.use(express.json());

//  FIXED PATH
app.use(express.static(path.join(__dirname, "../public")));

//  Home route
app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "../public", "index.html"));
});

//  API
app.post("/run", (req, res) => {
  const code = req.body.code;

  const exePath = require("path").join(__dirname, "compiler.exe");

  const process = exec(`"${exePath}"`, (error, stdout, stderr) => {
    if (error) return res.json({ output: error.message });
    if (stderr) return res.json({ output: stderr });

    res.json({ output: stdout });
  });

  process.stdin.write(code);
  process.stdin.end();
});
//  START SERVER
app.listen(5000, () => {
  console.log("Running at http://localhost:5000");

  // auto open browser
  require("child_process").exec("start http://localhost:5000");
});