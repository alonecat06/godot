const fs = require('fs');
const { JSDOM } = require('jsdom');

const file = process.argv[2];
if (!file) { console.error('Usage: node check.js <mdfile>'); process.exit(1); }

const content = fs.readFileSync(file, 'utf8');
const blocks = [];
const regex = /```mermaid\n([\s\S]*?)```/g;
let m;
while ((m = regex.exec(content)) !== null) {
  blocks.push(m[1].trim());
}

if (blocks.length === 0) {
  console.log('No mermaid blocks found.');
  process.exit(0);
}

console.log(`Found ${blocks.length} mermaid block(s).`);

async function validate() {
  const dom = new JSDOM('<!DOCTYPE html><html><body><div id="container"></div></body></html>');
  global.window = dom.window;
  global.document = dom.window.document;
  global.navigator = dom.window.navigator;

  const mermaid = require('mermaid');
  mermaid.initialize({ startOnLoad: false, securityLevel: 'loose' });

  let errors = 0;
  for (let i = 0; i < blocks.length; i++) {
    try {
      mermaid.mermaidAPI.parse(blocks[i]);
      console.log(`  Block ${i + 1}: OK`);
    } catch (e) {
      const msg = e.message || e.str || String(e);
      console.error(`  Block ${i + 1}: ERROR - ${msg.split('\n')[0]}`);
      errors++;
    }
  }
  if (errors > 0) {
    console.error(`\n${errors} block(s) have errors!`);
    process.exit(1);
  } else {
    console.log('\nAll blocks valid!');
  }
}

validate();
