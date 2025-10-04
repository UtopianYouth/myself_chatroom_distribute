import Head from "@/components/Head";
import styles from "@/styles/Home.module.css";
import Header from "@/components/Header";
import { Button, Link, FormHelperText } from "@mui/material";
import { useForm, SubmitHandler } from "react-hook-form";
import useLoading, { DontClearLoading } from "@/hooks/useLoading";
import { createAccount } from "@/lib/api";
import { ErrorId } from "@/lib/apiTypes";
import { useRouter } from "next/router";
import EmailInput from "@/components/EmailInput";
import PasswordInput from "@/components/PasswordInput";
import UsernameInput from "@/components/UsernameInput";
import { useCallback } from "react";
import FormCard from "@/components/FormCard";
import { setHasAuth } from "@/lib/hasAuth";

type Inputs = {
  username: string;
  email: string;
  password: string;
};

// The home page
export default function HomePage() {
  const router = useRouter();

  const {
    register,
    handleSubmit,
    setError,
    formState: { errors },
  } = useForm<Inputs>({ mode: "onTouched" });

  const { loading, launch } = useLoading();

  const onSubmit: SubmitHandler<Inputs> = useCallback(
    (inputs) => {
      launch(async () => {
        try {
          const { type } = await createAccount(inputs);
          switch (type) {
            case "ok":
              setHasAuth();
              router.push("/chat");
              return DontClearLoading;
            case ErrorId.EmailExists:
              setError("email", {
                type: "value",
                message: "This email address is already in use.",
              });
              break;
            case ErrorId.UsernameExists:
              setError("username", {
                type: "value",
                message:
                  "This username is already in use. Please pick a different one.",
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
    [router, setError, launch],
  );

  return (
    <>
      <Head />
      <div className={`${styles.bodycontainer} flex flex-col h-screen`}>
        <Header pageType="auth" />
        <div className="flex-1 flex items-center justify-center p-12">
          <div className="w-full max-w-md">
            <FormCard>
              <form
                className="flex-1 flex flex-col"
                onSubmit={handleSubmit(onSubmit)}
              >
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
                  className="flex-1 pb-2"
                />
                <UsernameInput
                  register={register}
                  name="username"
                  errorMessage={errors?.username?.message}
                  className="flex-1 pb-2"
                />
                {errors.root && (
                  <FormHelperText error>{errors.root.message}</FormHelperText>
                )}
                <div className="pt-8 flex justify-center">
                  <Button
                    type="submit"
                    disabled={loading}
                    sx={{
                      background: 'rgba(255, 255, 255, 0.2)',
                      backdropFilter: 'blur(10px)',
                      border: '1px solid rgba(255, 255, 255, 0.3)',
                      boxShadow: '0 4px 6px rgba(0, 0, 0, 0.1)',
                      color: 'black',
                      padding: '10px 20px',
                      borderRadius: '8px',
                      '&:hover': {
                        background: 'rgba(255, 255, 255, 0.3)',
                      },
                    }}
                  >
                    {loading ? "创建账号中..." : "创建账号"}
                  </Button>
                </div>
                <div className="pt-8 flex justify-center">
                  <p className="p-0 m-0 text-sm">
                    你已经有账号了？ 去{" "}
                    <Link href="/login">登录</Link>
                  </p>
                </div>
              </form>
            </FormCard>
          </div>
        </div>
      </div>
    </>
  );
}
