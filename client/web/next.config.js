/**
 * @type {import('next').NextConfig}
 */
const nextConfig = {
  async rewrites() {
    return [
      {
        source: "/api/:path(.*)",
        destination: `${
          process.env.SERVER_BASE_URL || "http://localhost:8080"
        }/api/:path`,
      },
    ];
  },
  eslint: {
    ignoreDuringBuilds: true,
  },
};

module.exports = nextConfig;
