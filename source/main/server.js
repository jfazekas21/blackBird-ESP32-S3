const express = require('express');
const path = require('path');
const app = express();
const port = 8899;

// Serve static files from the root directory
app.use(express.static(__dirname));

// Define a route for the root URL
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'upload_script2.html'));
});
app.get('/updates', (req, res) => {
  res.sendFile(path.join(__dirname, 'upload_script1.html'));
});
// Your API routes and other configurations go here

app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*'); // Allow any origin during development
  next();
});

// Your API endpoint to serve the JSON data
app.get('/config', (req, res) => {
  const jsonData = {
    // Your JSON data here
  };

  res.json(jsonData);
});

const PORT = 8899;
app.listen(PORT, () => {
  console.log(`Server running on http://localhost:${PORT}`);
});
