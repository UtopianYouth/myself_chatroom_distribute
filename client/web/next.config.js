/**
 * @type {import('next').NextConfig}
 */
const nextConfig = {
  async rewrites() {
    return [
      {
        source: "/api/:path(.*)",
        destination: `${
          process.env.SERVER_BASE_URL || "http://chatroom-app:8080"
        }/api/:path`,
      },
    ];
  },
  eslint: {
    ignoreDuringBuilds: true,
  },
};

module.exports = nextConfig;
