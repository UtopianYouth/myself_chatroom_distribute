import Head from "@/components/Head";
import styles from "@/styles/Home.module.css";
import Header from "@/components/Header";
import { Button, Link, FormHelperText } from "@mui/material";
import { useCallback, useEffect, useState } from "react";
import { useForm, SubmitHandler } from "react-hook-form";
import useLoading, { DontClearLoading } from "@/hooks/useLoading";
import { login } from "@/lib/api";
import { ErrorId } from "@/lib/apiTypes";
import { useRouter } from "next/router";
import EmailInput from "@/components/EmailInput";
import PasswordInput from "@/components/PasswordInput";
import FormCard from "@/components/FormCard";
import { hasAuth, setHasAuth } from "@/lib/hasAuth";

type Inputs = {
  email: string;
  password: string;
};

const LoginScreen = () => {
  const router = useRouter();
  const [showRegisteredMessage, setShowRegisteredMessage] = useState(false);

  // 检查是否是从注册页面跳转过来的
  useEffect(() => {
    if (router.query.registered === "true") {
      setShowRegisteredMessage(true);
      // 10秒后自动隐藏提示
      const timer = setTimeout(() => {
        setShowRegisteredMessage(false);
      }, 10000);

      return () => clearTimeout(timer);
    }
  }, [router.query.registered]);

  const {
    register,
    handleSubmit,
    setError,
    formState: { errors },
  } = useForm<Inputs>({ mode: "onTouched" });

  const { loading, launch } = useLoading();
  const onSubmit: SubmitHandler<Inputs> = useCallback(
    async (inputs) => {
      await launch(async () => {
        try {
          const { type } = await login(inputs);
          switch (type) {
            case "ok":
              setHasAuth();
              router.push("/chat");
              return DontClearLoading;
            case ErrorId.LoginFailed:
              setError("root", {
                type: "value",
                message: "无效的用户名、密码或权限未开通",
              });
              break;
          }
        } catch (err) {
          setError("root", {
            type: "value",
            message: "There was an unexpected error. Please try again later.",
          });
        }
      });
    },
    [router, launch, setError],
  );

  return (
    <>
      <Head />
      <div className={`${styles.bodycontainer} flex flex-col h-screen`}>
        <Header pageType="auth" />
        <div
          className="flex-1 flex items-center justify-center p-12"
        >
          <div className="w-full max-w-md">
            <FormCard>
              <form
                onSubmit={handleSubmit(onSubmit)}
                className="flex-1 flex flex-col"
              >
                {/* 注册成功提示消息 - 艺术字样式 */}
                {showRegisteredMessage && (
                  <div
                    className="w-full mb-6 flex justify-center"
                    style={{
                      animation: 'fadeInOut 10s ease-in-out',
                    }}
                  >
                    <div
                      style={{
                        color: 'rgba(25, 118, 210, 0.8)',
                        fontSize: 'clamp(0.65rem, 2.5vw, 0.875rem)',
                        fontWeight: '600',
                        fontFamily: 'Arial, sans-serif',
                        letterSpacing: '0.01em',
                        padding: '8px 12px',
                        borderRadius: '8px',
                        backgroundColor: 'rgba(255, 255, 255, 0.9)',
                        boxShadow: '0 2px 12px rgba(25, 118, 210, 0.2)',
                        textAlign: 'center',
                        border: '1px solid rgba(25, 118, 210, 0.3)',
                        display: 'inline-block',
                      }}
                    >
                      注册成功，联系作者开放系统使用权限吧！
                    </div>
                  </div>
                )}

                <EmailInput
                  register={register}
                  name="email"
                  errorMessage={errors?.email?.message}
                  className="flex-1 pb-2"
                />
                <PasswordInput
                  register={register}
                  name="password"
                  errorMessage={errors?.password?.message}
                  className="flex-1 pb-2 "
                />
                {errors.root && (
                  <FormHelperText error>{errors.root.message}</FormHelperText>
                )}
                <div className="pt-8 flex justify-center">
                  <Button variant="contained" type="submit" disabled={loading}>
                    {loading ? "登录中..." : "登录"}
                  </Button>
                </div>
                <div className="pt-8 flex justify-center">
                  <p className="p-0 m-0 text-sm">
                    你还没有账号? 去{" "}
                    <Link href="/">创建账号</Link>
                  </p>
                </div>
              </form>
            </FormCard>
          </div>
        </div>
      </div>
      <style jsx>{`
        @keyframes fadeInOut {
          0% {
            opacity: 0;
            transform: translateY(-10px);
          }
          10% {
            opacity: 1;
            transform: translateY(0);
          }
          90% {
            opacity: 1;
            transform: translateY(0);
          }
          100% {
            opacity: 0;
            transform: translateY(-10px);
          }
        }
      `}</style>
    </>
  );
};

export default function LoginPage() {
  const router = useRouter();

  const [loading, setLoading] = useState(true);

  useEffect(() => {
    if (hasAuth()) {
      router.replace("/chat");
    } else {
      setLoading(false);
    }
  }, [router, setLoading]);

  if (loading) return <></>;

  return <LoginScreen />;
}