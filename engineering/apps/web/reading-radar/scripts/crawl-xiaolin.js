/**
 * crawl-xiaolin.js
 *
 * 爬取 xiaolincoding.com 的图解系列文章
 * - 解析 sitemap.xml 获取所有 URL
 * - 逐页爬取 HTML，提取正文
 * - 保存原始 HTML 到 scripts/data/xiaolin-raw/{topic}/{itemId}.html
 * - 输出结构化 JSON 到 scripts/data/xiaolin-raw/extracted.json
 *
 * 用法: node scripts/crawl-xiaolin.js [--topics network os agent claudecode]
 *       默认爬取 network + os（MySQL/Redis 已有）
 */

const https = require('https');
const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const RAW_DIR = path.join(ROOT, 'scripts', 'data', 'xiaolin-raw');
const TOPICS_TO_CRAWL = process.argv.slice(2).filter(a => !a.startsWith('--'));

// 如果没有指定 topic，默认爬取 network 和 os
const TOPIC_MAP = {
  network: {
    baseUrl: 'https://www.xiaolincoding.com',
    section: 'network',
    stack: 'grok',
    quadrant: 'systems',
    ring: 'basic',
    cssSelector: '#article-content',
    titleSelector: 'h1.title',
  },
  os: {
    baseUrl: 'https://www.xiaolincoding.com',
    section: 'os',
    stack: 'grok',
    quadrant: 'systems',
    ring: 'basic',
    cssSelector: '#article-content',
    titleSelector: 'h1.title',
  },
  agent: {
    baseUrl: 'https://xiaolinnote.com',
    section: 'agent',
    stack: 'grok',
    quadrant: 'algorithms',
    ring: 'basic',
    cssSelector: '#article-content',
    titleSelector: 'h1.title',
  },
  claudecode: {
    baseUrl: 'https://xiaolinnote.com',
    section: 'claudecode',
    stack: 'grok',
    quadrant: 'engineering',
    ring: 'basic',
    cssSelector: '#article-content',
    titleSelector: 'h1.title',
  },
};

const DEFAULT_TOPICS = ['network', 'os'];
const topics = TOPIC_MAP;
const selectedTopics = TOPICS_TO_CRAWL.length > 0 ? TOPICS_TO_CRAWL : DEFAULT_TOPICS;

console.log('=== xiaolincoding 爬虫 ===');
console.log('目标专题:', selectedTopics.join(', '));

// ── 工具函数 ──

function fetchUrl(url) {
  return new Promise((resolve, reject) => {
    const isHttps = url.startsWith('https');
    const mod = isHttps ? https : http;
    const req = mod.get(url, { headers: { 'User-Agent': 'Mozilla/5.0 (compatible; ReadingRadarBot/1.0)' } }, (res) => {
      if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        const redirectUrl = res.headers.location.startsWith('http')
          ? res.headers.location
          : url.replace(/\/[^/]*$/, '') + '/' + res.headers.location;
        fetchUrl(redirectUrl).then(resolve, reject);
        return;
      }
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => resolve({ statusCode: res.statusCode, html: data }));
    });
    req.on('error', reject);
    req.setTimeout(30000, () => { req.destroy(); reject(new Error('Timeout: ' + url)); });
  });
}

function sleep(ms) {
  return new Promise(r => setTimeout(r, ms));
}

// 最简单的 HTML 提取（无外部依赖）
function extractTitle(html, selector) {
  const tagName = selector.split('.')[0];
  const className = selector.split('.')[1];
  const pattern = new RegExp('<' + tagName + '[^>]*class=["\'][^"\']*' + className + '[^"\']*["\'][^>]*>([^<]+)</' + tagName + '>', 'i');
  const match = html.match(pattern);
  return match ? match[1].trim() : '';
}

function extractMetaTags(html, name) {
  const match = html.match(new RegExp('<meta[^>]*name=["\']' + name + '[^>]*content=["\']([^"\']+)["\']', 'i'));
  return match ? match[1] : '';
}

function extractDescription(html) {
  let desc = extractMetaTags(html, 'description');
  if (desc) return desc;
  desc = extractMetaTags(html, 'og:description');
  if (desc) return desc;
  const pMatch = html.match(/<p[^>]*>([^<]+)<\/p>/);
  return pMatch ? pMatch[1].trim() : '';
}

// ── 爬取 sitemap ──

async function crawlSitemap(topicConfig) {
  const sitemapUrl = topicConfig.baseUrl + '/sitemap.xml';
  console.log(`\n[${topicConfig.section}] 获取 sitemap: ${sitemapUrl}`);

  const { html } = await fetchUrl(sitemapUrl);
  const urls = [];
  const host = new URL(topicConfig.baseUrl).hostname;

  // Extract all URLs from sitemap (all URLs are in one line in this sitemap)
  const locRegex = new RegExp(`<loc>https://${host}/${section}/[a-zA-Z0-9_/]+\\.html</loc>`, 'gi');
  const matches = html.match(locRegex) || [];
  matches.forEach(m => {
    const url = m.replace(/<loc>|<\/loc>/gi, '').trim();
    urls.push(url);
  });

  console.log(`  找到 ${urls.length} 篇文章`);
  return urls;
}

// ── 爬取单篇文章 ──

async function crawlArticle(url, topicConfig, delayMs = 500) {
  try {
    const { html } = await fetchUrl(url);
    await sleep(delayMs);

    // Extract title
    const title = extractTitle(html, 'h1.title') || extractMetaTags(html, 'og:title') || url.split('/').pop().replace('.html', '');

    // Extract description
    const desc = extractDescription(html);

    // Extract body content - try common selectors
    let body = '';
    const bodyMatch = html.match(/id=["']article-content["'][^>]*>([\s\S]*?)<\/div>\s*<\/article>/);
    if (bodyMatch) {
      body = bodyMatch[1];
    } else {
      // Fallback: try to find the main content area
      const fallbackMatch = html.match(/<article[^>]*>([\s\S]*?)<\/article>/);
      if (fallbackMatch) {
        body = fallbackMatch[1];
      } else {
        // Last resort: strip script/style tags
        body = html.replace(/<script[\s\S]*?<\/script>/gi, '')
                   .replace(/<style[\s\S]*?<\/style>/gi, '')
                   .replace(/<[^>]+>/g, '\n')
                   .replace(/\n+/g, '\n')
                   .trim()
                   .substring(0, 5000);
      }
    }

    // Extract image URLs
    const images = [...html.matchAll(/<img[^>]*src=["']([^"']+)["']/gi)].map(m => m[1]);

    return {
      url,
      topic: topicConfig.section,
      title,
      description: desc,
      bodyHtml: body,
      images,
      stack: topicConfig.stack,
      quadrant: topicConfig.quadrant,
      ring: topicConfig.ring,
    };
  } catch (err) {
    console.error(`  [ERROR] ${url}: ${err.message}`);
    return { url, topic: topicConfig.section, error: err.message };
  }
}

// ── 主流程 ──

async function main() {
  fs.mkdirSync(RAW_DIR, { recursive: true });

  const allResults = [];
  let totalArticles = 0;
  let successCount = 0;
  let errorCount = 0;

  for (const topicName of selectedTopics) {
    const config = TOPIC_MAP[topicName];
    if (!config) {
      console.log(`跳过未知专题: ${topicName}`);
      continue;
    }

    const urls = await crawlSitemap(config);
    totalArticles += urls.length;

    console.log(`\n[${topicName}] 开始爬取 ${urls.length} 篇文章...`);

    // Create topic directory
    const topicDir = path.join(RAW_DIR, config.section);
    fs.mkdirSync(topicDir, { recursive: true });

    for (let i = 0; i < urls.length; i++) {
      const url = urls[i];
      const itemId = url.split('/').pop().replace('.html', '');
      const result = await crawlArticle(url, config, i < urls.length - 1 ? 800 : 100);

      if (result.bodyHtml) {
        fs.writeFileSync(path.join(topicDir, itemId + '.html'), result.bodyHtml, 'utf-8');
        successCount++;
      } else {
        errorCount++;
      }

      allResults.push(result);
      process.stdout.write(`\r  [${topicName}] ${i + 1}/${urls.length}`);
    }
    console.log();
  }

  // Save extracted results
  const output = {
    crawledAt: new Date().toISOString(),
    totalArticles,
    successCount,
    errorCount,
    topics: selectedTopics,
    items: allResults,
  };

  const outputPath = path.join(RAW_DIR, 'extracted.json');
  fs.writeFileSync(outputPath, JSON.stringify(output, null, 2), 'utf-8');
  console.log(`\n=== 爬取完成 ===`);
  console.log(`总文章: ${totalArticles}`);
  console.log(`成功: ${successCount}`);
  console.log(`失败: ${errorCount}`);
  console.log(`结果: ${outputPath}`);
  console.log(`原始文件: ${RAW_DIR}/`);
}

main().catch(err => {
  console.error('Fatal error:', err);
  process.exit(1);
});
