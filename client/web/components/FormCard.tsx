import { PropsWithChildren } from "react";

import { PropsWithChildren } from "react";

export default function FormCard({
  children,
}: PropsWithChildren<{}>) {
  return (
    <div
      className="bg-white/10 backdrop-blur-md rounded-2xl p-12 flex flex-col"
      style={{ minWidth: "50%" }}
    >
      <div className="flex justify-center">
        <div
          className="pt-8 pr-5 pl-5 pb-3 flex-1 flex"
          style={{ maxWidth: "40em" }}
        >
          {children}
        </div>
      </div>
    </div>
  );
}
