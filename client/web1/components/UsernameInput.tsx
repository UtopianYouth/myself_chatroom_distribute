import { TextField } from "@mui/material";
import { FieldValues, Path, UseFormRegister } from "react-hook-form";

const minLength = 4;

export default function UsernameInput<TFieldValues extends FieldValues>({
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
      label="用户名(公开)"
      required
      inputProps={{ maxLength: 100, "aria-label": "username" }}
      {...register(name, {
        required: {
          value: true,
          message: "必填项",
        },
        minLength: {
          value: minLength,
          message: `用户名应至少包含 ${minLength} 个字符。`,
        },
      })}
      error={!!errorMessage}
      helperText={errorMessage}
      className={className}
    />
  );
}
