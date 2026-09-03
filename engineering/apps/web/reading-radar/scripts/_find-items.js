var fs = require("fs");
var lines = fs.readFileSync("data/app/practice-data.js", "utf-8").split("\n");
var start = null,
  catId = null;
for (var i = 0; i < lines.length; i++) {
  var m = lines[i].match(/^\s+id:\s+"([\w-]+)"/);
  if (m) {
    start = i + 2;
    catId = m[1];
  }
  if (start && lines[i].match(/^\s+\],\s*$/) && catId) {
    console.log(catId + ": items lines " + start + "-" + i);
    start = null;
    catId = null;
  }
}
