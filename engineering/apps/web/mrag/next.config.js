/** @type {import('next').NextConfig} */
const nextConfig = {
  reactStrictMode: true,
  swcMinify: true,
  // 跨域配置（开发环境）
  async rewrites() {
    return [
      {
        source: '/api/v1/:path*',
        destination: `http://localhost:8080/api/v1/:path*`,
      },
    ];
  },
};

export default nextConfig;
