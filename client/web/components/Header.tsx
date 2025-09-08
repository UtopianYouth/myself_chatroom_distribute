import React from "react";
import Image from "next/image";
import boostLogo from "@/public/boost.jpg";
import { clearHasAuth } from "@/lib/hasAuth";

// The common Header with the Boost logo shown in all pages

const links = [
  { text: "零声教育", href: "https://www.0voice.com/" },
  { text: "VIP课程", href: "https://it.0voice.com/" },
];

export default function Header() {
  // 处理退出按钮点击事件
  const handleLogout = () => {
    // 这里可以添加退出逻辑，例如清除用户登录状态、跳转到登录页等
    console.log("退出登录");
    alert("退出登录成功！");
    clearHasAuth();
    // router.replace("/login");
    // 例如：跳转到登录页
    window.location.href = "/login";
  };

  return (
    <div className="flex m-3">
      {/* Boost logo */}
      <Image src={boostLogo} height={60} alt="Boost logo" />

      {/* 链接 */}
      <div className="flex-1 flex justify-end items-center">
        {links.map(({ text, href }) => (
          <div key={href} className="flex flex-col justify-center px-12">
            <a className="no-underline text-2xl text-black" href={href}>
              {text}
            </a>
          </div>
        ))}

        {/* 退出按钮 */}
        <button
          onClick={handleLogout}
          className="ml-12 px-6 py-2 bg-blue-500 text-white text-xl rounded hover:bg-blue-600 transition-colors"
        >
          退出
        </button>
      </div>
    </div>
  );
}