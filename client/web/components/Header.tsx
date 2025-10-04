import React, { useState, useEffect } from "react";
import Image from "next/image";
import boostLogo from "@/public/boost.jpg";
import { clearHasAuth } from "@/lib/hasAuth";

interface HeaderProps {
  onCreateRoom?: () => void;
  currentUserId?: string;
  username?: string;
  pageType?: 'auth' | 'chat';
}

export default function Header({ onCreateRoom, currentUserId, username, pageType = 'chat' }: HeaderProps) {
  const [hoveredDropdown, setHoveredDropdown] = useState<string | null>(null);
  const [showModal, setShowModal] = useState<{type: string, content: string} | null>(null);

  // 处理退出按钮点击事件
  const handleLogout = () => {
    console.log("退出登录");
    alert("退出登录成功！");
    clearHasAuth();
    window.location.href = "/login";
  };

  // 处理鼠标悬停
  const handleMouseEnter = (menu: string) => {
    setHoveredDropdown(menu);
  };

  // 处理鼠标离开
  const handleMouseLeave = () => {
    setHoveredDropdown(null);
  };

  // 显示模态框
  const showInfoModal = (type: string, content: string) => {
    setShowModal({ type, content });
    setHoveredDropdown(null);
  };

  // 关闭模态框
  const closeModal = () => {
    setShowModal(null);
  };

  // 文本自动超链接（URL/邮箱）
  const linkify = (text: string) => {
    const urlRegex = /(https?:\/\/[^\s]+)/g;
    const emailRegex = /([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})/g;
    const parts = text.split(/(\n|https?:\/\/[^\s]+|[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})/g);
    return (
      <>
        {parts.map((part, i) => {
          if (part === "\n") return <br key={i} />;
          if (urlRegex.test(part)) {
            return (
              <a
                key={i}
                href={part}
                target="_blank"
                rel="noopener noreferrer"
                className="text-blue-600 hover:underline border-none"
              >
                {part}
              </a>
            );
          }
          if (emailRegex.test(part)) {
            return (
              <a
                key={i}
                href={`mailto:${part}`}
                className="text-blue-600 hover:underline border-none"
              >
                {part}
              </a>
            );
          }
          return <span key={i}>{part}</span>;
        })}
      </>
    );
  };

  // 绑定 ESC 键关闭模态框
  useEffect(() => {
    if (!showModal) return;
    const onKeyDown = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        closeModal();
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => {
      window.removeEventListener("keydown", onKeyDown);
    };
  }, [showModal]);

  // 获取用户首字母
  const getUserInitial = () => {
    if (!username) return "U";
    return username.charAt(0).toUpperCase();
  };

  const isAuthPage = pageType === 'auth';

  const dropdownContainerClass = isAuthPage
    ? "bg-white/20 backdrop-blur-md rounded-lg p-2 border border-white/30 shadow-lg"
    : "bg-white rounded-lg p-2 border-none shadow-lg";

  const dropdownItemClass = isAuthPage
    ? "block text-base text-white hover:bg-white/20 rounded px-4 py-3 no-underline whitespace-nowrap text-center"
    : "block text-base text-gray-700 hover:bg-gray-100 rounded px-4 py-3 no-underline whitespace-nowrap text-center";

  return (
    <>
      {/* 主头部 */}
      <div className={`relative z-10 flex items-center justify-between px-6 py-3 ${isAuthPage ? 'bg-transparent' : 'bg-white border-b border-gray-200 shadow-sm'}`}>
        {/* 左侧：校徽和学校名称 */}
        <div className="flex items-center space-x-3">
          <Image src={boostLogo} height={32} width={32} alt="深圳大学校徽" priority />
          <span className={`text-lg font-semibold ${isAuthPage ? 'text-white' : 'text-gray-800'}`}>Shenzhen University</span>
        </div>

        {/* 中间：导航菜单 */}
        <nav className="absolute left-1/2 transform -translate-x-1/2 flex items-center space-x-8">
          {/* 介绍下拉菜单 */}
          <div 
            className="relative"
            onMouseEnter={() => handleMouseEnter('introduction')}
            onMouseLeave={handleMouseLeave}
          >
            <a
              onClick={(e) => e.preventDefault()}
              className={`relative px-4 py-2 hover:text-blue-600 transition-colors duration-200 text-lg cursor-pointer ${isAuthPage ? 'text-white' : 'text-gray-800'}`}
            >
              分享
              <span className={`absolute bottom-0 left-0 w-full h-1 bg-blue-300 transform scale-x-0 origin-left transition-transform duration-200 ease-out ${hoveredDropdown === 'introduction' ? 'scale-x-100' : ''}`}></span>
            </a>
            {hoveredDropdown === 'introduction' && (
              <div className="absolute top-full left-1/2 transform -translate-x-1/2 pt-2 z-50">
                <div className={dropdownContainerClass}>
                  <a href="https://blog.csdn.net/fantasticHQ?spm=1000.2115.3001.5343" className={dropdownItemClass} target="_blank" rel="noopener noreferrer">
                    Blog
                  </a>

                  <a href="https://github.com/UtopianYouth" className={dropdownItemClass} target="_blank" rel="noopener noreferrer">
                    GitHub
                  </a>

                  <a href="./resume.pdf" className={dropdownItemClass} target="_blank" rel="noopener noreferrer">
                    Resume
                  </a>

                </div>
              </div>
            )}
          </div>

          {/* 房间管理下拉菜单 */}
          {!isAuthPage && username && (
            <div 
              className="relative"
              onMouseEnter={() => handleMouseEnter('roomManagement')}
              onMouseLeave={handleMouseLeave}
            >
              <a
                onClick={(e) => e.preventDefault()}
                className="relative px-4 py-2 text-gray-800 hover:text-blue-600 transition-colors duration-200 text-lg cursor-pointer"
              >
                房间管理
                <span className={`absolute bottom-0 left-0 w-full h-1 bg-blue-300 transform scale-x-0 origin-left transition-transform duration-200 ease-out ${hoveredDropdown === 'roomManagement' ? 'scale-x-100' : ''}`}></span>
              </a>
              {hoveredDropdown === 'roomManagement' && (
                <div className="absolute top-full left-1/2 transform -translate-x-1/2 pt-2 z-50">
                  <div className={dropdownContainerClass}>
                    <a
                      href="#"
                      onClick={(e) => {
                        e.preventDefault();
                        onCreateRoom?.();
                      }}
                      className={dropdownItemClass}
                    >
                      创建房间
                    </a>
                    <a href="#" className={dropdownItemClass}>
                      删除房间
                    </a>
                    <a href="#" className={dropdownItemClass}>
                      修改房间
                    </a>
                  </div>
                </div>
              )}
            </div>
          )}

          {/* 关于下拉菜单 */}
          <div 
            className="relative"
            onMouseEnter={() => handleMouseEnter('about')}
            onMouseLeave={handleMouseLeave}
          >
            <a
              onClick={(e) => e.preventDefault()}
              className={`relative px-4 py-2 hover:text-blue-600 transition-colors duration-200 text-lg cursor-pointer ${isAuthPage ? 'text-white' : 'text-gray-800'}`}
            >
              关于
              <span className={`absolute bottom-0 left-0 w-full h-1 bg-blue-300 transform scale-x-0 origin-left transition-transform duration-200 ease-out ${hoveredDropdown === 'about' ? 'scale-x-100' : ''}`}></span>
            </a>
            {hoveredDropdown === 'about' && (
              <div className="absolute top-full left-1/2 transform -translate-x-1/2 pt-2 z-50">
                <div className={dropdownContainerClass}>
                  <a
                    href="#"
                    onClick={(e) => {
                      e.preventDefault();
                      showInfoModal('product', 'This is a distributed chat room implemented using the Kafka and gRPC message push system.\n\nCode: https://github.com/UtopianYouth/myself_chatroom_distribute');
                    }}
                    className={dropdownItemClass}
                  >
                    产品介绍
                  </a>
                  <a
                    onClick={(e) => {
                      e.preventDefault();
                      showInfoModal('contact', 'Author: UtopianYouth\n\nEmail: heqing2023@email.szu.edu.cn\n\nGitHub: https://github.com/UtopianYouth');
                    }}
                    className={dropdownItemClass}
                  >
                    联系作者
                  </a>
                </div>
              </div>
            )}
          </div>
        </nav>

        {/* 右侧：用户操作按钮 */}
        <div className="flex items-center space-x-3">
          {/* 用户头像按钮 - 仅在登录时显示 */}
          {username && (
            <button 
              className="w-10 h-10 rounded-full bg-gradient-to-r from-blue-500 to-blue-600 text-white font-semibold flex items-center justify-center transition-all duration-200 border-none"
              title={username}
            >
              {getUserInitial()}
            </button>
          )}

          {/* 退出按钮 */}
          {username && (
            <button
              onClick={handleLogout}
              className={`w-10 h-10 rounded-full flex items-center justify-center transition-colors duration-200 border-none ${isAuthPage ? 'text-white hover:bg-white/20' : 'bg-transparent text-gray-600 hover:bg-gray-100'}`}
              title="退出登录"
            >
              <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M17 16l4-4m0 0l-4-4m4 4H7m6 4v1a3 3 0 01-3 3H6a3 3 0 01-3-3V7a3 3 0 013-3h4a3 3 0 013 3v1" />
              </svg>
            </button>
          )}
        </div>
      </div>

      {/* 模态框 */}
      {showModal && (
        <div className="fixed inset-0 bg-white/50 backdrop-blur-sm flex items-center justify-center z-50">
          <div className="relative bg-white/10 backdrop-blur-xl rounded-2xl shadow-2xl border border-white/20 max-w-xl w-[92%] md:w-[560px] mx-4">
            <button
              className="absolute top-3 right-3 p-2 text-gray-400 hover:text-gray-600 transition-colors bg-transparent border-none focus:outline-none"
              aria-label="关闭"
              onClick={closeModal}
            >
              <svg className="w-5 h-5" viewBox="0 0 24 24" fill="none" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M6 6l12 12M6 18L18 6" />
              </svg>
            </button>


            <div className="p-6 md:p-8 max-h-[70vh] overflow-y-auto">
              <h3 className="text-lg font-semibold text-gray-800 mb-4">
                {showModal.type === 'product' ? 'Project Introduction' : 'Contact Me'}
              </h3>
              <div className="text-gray-600 whitespace-pre-line">
                {linkify(showModal.content)}
              </div>
              <div className="mt-6 flex justify-end">
                <button
                  onClick={closeModal}
                  className="px-4 py-2 bg-blue-500 text-white rounded-lg hover:bg-blue-600 transition-colors duration-200 border-none focus:outline-none"
                >
                  关闭
                </button>
              </div>
            </div>
          </div>
        </div>
      )}
    </>
  );
}