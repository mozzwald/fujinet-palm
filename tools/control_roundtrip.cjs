/* Serial palm-sync only: verify installed RAM PRC and install/read its control
 * in one connection. Never installs a PRC, changes the clock or resets PalmOS. */
const fs = require('fs');
const path = require('path');
const root = path.join(__dirname, 'palm-sync');
const {createSyncServerAndRunSync} = require(path.join(root, 'dist/sync-servers/sync-server-utils'));
const {readRawDb} = require(path.join(root, 'dist/sync-utils/read-db'));
const {writeRawDb} = require(path.join(root, 'dist/sync-utils/write-db'));
const {RawPdbDatabase} = require(path.join(root, 'node_modules/palm-pdb'));

async function main() {
  const [port, control, appName, output] = process.argv.slice(2);
  if (!port || !control || !appName || !output) throw new Error('Expected serial port, control PDB, RAM app name, output directory');
  const data = fs.readFileSync(control);
  if (data.length < 80 || data.subarray(0,32).toString().replace(/\0.*$/s,'') !== 'ROMProbeControl' ||
      data.subarray(60,68).toString() !== 'VCTLFNVD' || (data.readUInt16BE(32) & 0x21)) {
    throw new Error('Only the non-reset FNVD ROMProbeControl record database may be installed');
  }
  await createSyncServerAndRunSync(`serial:${port}`, async connection => {
    const app = await readRawDb(connection, appName);
    fs.writeFileSync(path.join(output, 'installed-app.prc'), app.serialize(), {flag:'wx'});
    await writeRawDb(connection, RawPdbDatabase.from(data), {overwrite:true});
    const returned = await readRawDb(connection, 'ROMProbeControl');
    fs.writeFileSync(path.join(output, 'ROMProbeControl.pdb'), returned.serialize(), {flag:'wx'});
  }, {});
  process.exit(0); // Same serialport cleanup workaround as the existing CLI.
}
main().catch(error => { console.error(error.message); process.exit(1); });
