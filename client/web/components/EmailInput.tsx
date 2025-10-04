import { TextField } from "@mui/material";
import { FieldValues, Path, UseFormRegister } from "react-hook-form";

const emailRegex =
  /^(([^<>()[\]\\.,;:\s@"]+(\.[^<>()[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;

export default function EmailInput<TFieldValues extends FieldValues>({
  register,
  name,
  errorMessage,
  className = "",
}: {
  register: UseFormRegister<TFieldValues>;
  name: Path<TFieldValues>;
  errorMessage?: string;
  className?: string;
}) {
  return (
    <TextField
      variant="standard"
      label="邮箱"
      required
      inputProps={{ maxLength: 100, "aria-label": "email" }}
      InputProps={{
        sx: {
          '& input:-webkit-autofill': {
            transition: 'background-color 5000s ease-in-out 0s',
            WebkitBoxShadow: '0 0 0 1000px transparent inset !important',
            WebkitTextFillColor: '#000 !important',
          },
        },
      }}
      type="email"
      {...register(name, {
        required: {
          value: true,
          message: "必填项",
        },
        pattern: {
          value: emailRegex,
          message: "请输入一个有效的邮箱.",
        },
      })}
      error={!!errorMessage}
      helperText={errorMessage}
      className={className}
    />
  );
}
