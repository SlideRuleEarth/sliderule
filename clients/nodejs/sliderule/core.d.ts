import https from 'https';

// Define types for the constants
export const INT8: number;
export const INT16: number;
export const INT32: number;
export const INT64: number;
export const UINT8: number;
export const UINT16: number;
export const UINT32: number;
export const UINT64: number;
export const BITFIELD: number;
export const FLOAT: number;
export const DOUBLE: number;
export const TIME8: number;
export const STRING: number;
export const USER: number;

// Define type for fieldtypes
export const fieldtypes: {
  [key: string]: { code: number; size: number };
};

// Define type for the init function
export function init(config: {
  domain?: string;
  organization?: string;
  protocol?: https;
  verbose?: boolean;
  desired_nodes?: any; // Replace 'any' with a more specific type if possible
  time_to_live?: number;
  timeout?: number;
}): void;

// Define type for the source function
export function source(
  api: string,
  parm?: any, // Replace 'any' with a more specific type if possible
  stream?: boolean,
  callbacks?: { [key: string]: (...args: any[]) => void }
): Promise<any>; // Replace 'any' with a more specific return type if possible

// Define type for the authenticate function
export function authenticate(
  ps_username?: string | null,
  ps_password?: string | null
): Promise<number | undefined>;

// Define type for the get_version function
export function get_version(): Promise<{
  client: { version: string };
  organization: string;
  [key: string]: any; // Additional dynamic properties
}>;

// Define type for the get_values function
export function get_values(bytearray: Uint8Array, fieldtype: number): any[]; // Replace 'any' with a more specific type if possible

