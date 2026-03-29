const sharp = require('sharp');
const pngToIco = require('png-to-ico').default;
const fs = require('fs');
const path = require('path');

const svgPath = path.join(__dirname, 'icon.svg');
const icoPath = path.join(__dirname, 'icon.ico');

const sizes = [16, 32, 48, 64, 128, 256];

async function generateIco() {
    try {
        // Read SVG and create PNGs at different sizes
        const pngBuffers = await Promise.all(
            sizes.map(size =>
                sharp(svgPath)
                    .resize(size, size)
                    .png()
                    .toBuffer()
            )
        );

        // Convert to ICO
        const icoBuffer = await pngToIco(pngBuffers);
        fs.writeFileSync(icoPath, icoBuffer);
        console.log('Icon generated:', icoPath);
    } catch (err) {
        console.error('Error generating icon:', err.message);
        process.exit(1);
    }
}

generateIco();