const fs = require('fs');
const path = require('path');

// Create C:\VIDEO directory
const videoDir = 'C:\\VIDEO';
if (!fs.existsSync(videoDir)) {
    fs.mkdirSync(videoDir, { recursive: true });
}

// Create small test video files (few KB each - enough to test upload flow)
// Real video players won't play these, but the app will treat them as video files

const videos = [
    { name: '测试视频1_风景.mp4', size: 256 * 1024 },     // 256KB
    { name: '测试视频2_动物.mp4', size: 512 * 1024 },     // 512KB
    { name: '测试视频3_延时摄影.mp4', size: 384 * 1024 }, // 384KB
    { name: '测试视频4_夜景.mp4', size: 256 * 1024 },    // 256KB
];

// Create a subfolder
const subfolder = path.join(videoDir, '子文件夹');
if (!fs.existsSync(subfolder)) {
    fs.mkdirSync(subfolder, { recursive: true });
}

const moreVideos = [
    { name: '视频5_纪录片.mp4', size: 512 * 1024 },  // 512KB
    { name: '视频6_教程.mp4', size: 256 * 1024 },    // 256KB
];

function createVideoFile(filepath, sizeBytes) {
    // Create pseudo-random data that looks like compressed video
    const buf = Buffer.alloc(sizeBytes);
    const seed = filepath.split('').reduce((a, c) => a + c.charCodeAt(0), 0);

    // Write MP4 ftyp header at start
    buf.write('ftyp', 0);
    buf.write('isom', 4);
    buf.writeUInt32BE(1, 8);

    // Fill rest with pseudo-random data
    for (let i = 16; i < sizeBytes; i++) {
        buf[i] = (i * 17 + seed) % 256;
    }

    fs.writeFileSync(filepath, buf);
}

for (const v of videos) {
    const filepath = path.join(videoDir, v.name);
    createVideoFile(filepath, v.size);
    console.log(`Created: ${filepath} (${(v.size/1024).toFixed(0)}KB)`);
}

for (const v of moreVideos) {
    const filepath = path.join(subfolder, v.name);
    createVideoFile(filepath, v.size);
    console.log(`Created: ${filepath} (${(v.size/1024).toFixed(0)}KB)`);
}

console.log(`\nTest videos created in ${videoDir}`);
console.log('Total: 6 files, approximately 2.2MB');