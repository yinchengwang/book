/**
 * 快速验证脚本 - 检查页面是否能正常加载
 */
const { chromium } = require('playwright');

async function checkPage() {
    // 使用系统已有的 Chromium
    const browser = await chromium.launch({
        headless: true,
        executablePath: 'C:\\Users\\yinch\\AppData\\Local\\ms-playwright\\chromium-1223\\chrome-win64\\chrome.exe'
    });
    const page = await browser.newPage();

    console.log('正在访问 http://localhost:5173/learn_deep ...');

    const errors = [];
    page.on('console', msg => {
        if (msg.type() === 'error') {
            errors.push(msg.text());
        }
    });

    try {
        await page.goto('http://localhost:5173/learn_deep', { timeout: 30000 });
        await page.waitForTimeout(5000);

        const title = await page.title();
        console.log('页面标题:', title);

        const bodyText = await page.evaluate(() => document.body.innerText.substring(0, 800));
        console.log('\n页面内容预览:');
        console.log(bodyText);

        if (errors.length > 0) {
            console.log('\n❌ 控制台错误:');
            errors.forEach(e => console.log('  -', e));
        } else {
            console.log('\n✅ 没有控制台错误');
        }

        await page.screenshot({ path: 'screenshot-learn-deep.png', fullPage: true });
        console.log('\n截图已保存到 screenshot-learn-deep.png');

    } catch (err) {
        console.error('❌ 页面加载失败:', err.message);
        if (errors.length > 0) {
            console.log('控制台错误:');
            errors.forEach(e => console.log('  -', e));
        }
    }

    await browser.close();
}

checkPage();
