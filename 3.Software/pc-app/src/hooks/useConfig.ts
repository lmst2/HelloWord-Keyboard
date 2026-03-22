import { useCallback, useEffect, useRef, useState } from "react";
import {
  ParamMeta,
  configGet,
  configSet,
  getParamRegistry,
} from "@/lib/tauri-commands";

export function useConfig() {
  const [registry, setRegistry] = useState<ParamMeta[]>([]);
  const [values, setValues] = useState<Record<number, number[]>>({});
  const debounceTimers = useRef<Record<number, ReturnType<typeof setTimeout>>>({});

  useEffect(() => {
    getParamRegistry()
      .then(setRegistry)
      .catch(() => {});
  }, []);

  const readParam = useCallback(async (target: string, param: number) => {
    try {
      const result = await configGet(target, param);
      setValues((prev) => ({ ...prev, [param]: result.value }));
      return result.value;
    } catch {
      return null;
    }
  }, []);

  const writeParam = useCallback(
    (target: string, param: number, value: number | boolean) => {
      const key = param;
      if (debounceTimers.current[key]) clearTimeout(debounceTimers.current[key]);
      debounceTimers.current[key] = setTimeout(async () => {
        try {
          await configSet(target, param, value);
          setValues((prev) => ({
            ...prev,
            [param]: typeof value === "boolean" ? [value ? 1 : 0] : [value],
          }));
        } catch (e) {
          console.error("Config set failed:", e);
        }
      }, 150);
    },
    []
  );

  const categories = Array.from(new Set(registry.map((p) => p.category))).sort();

  return { registry, values, categories, readParam, writeParam };
}
